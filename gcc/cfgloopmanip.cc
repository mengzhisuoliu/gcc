/* Loop manipulation code for GNU compiler.
   Copyright (C) 2002-2025 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "cfganal.h"
#include "cfgloop.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "tree-ssa-loop-manip.h"
#include "dumpfile.h"
#include "sreal.h"

static void copy_loops_to (class loop **, int,
			   class loop *);
static void loop_redirect_edge (edge, basic_block);
static void remove_bbs (basic_block *, int);
static bool rpe_enum_p (const_basic_block, const void *);
static int find_path (edge, basic_block **);
static void fix_loop_placements (class loop *, bool *, bitmap);
static bool fix_bb_placement (basic_block);
static void fix_bb_placements (basic_block, bool *, bitmap);

/* Checks whether basic block BB is dominated by DATA.  */
static bool
rpe_enum_p (const_basic_block bb, const void *data)
{
  return dominated_by_p (CDI_DOMINATORS, bb, (const_basic_block) data);
}

/* Remove basic blocks BBS.  NBBS is the number of the basic blocks.  */

static void
remove_bbs (basic_block *bbs, int nbbs)
{
  int i;

  for (i = 0; i < nbbs; i++)
    delete_basic_block (bbs[i]);
}

/* Find path -- i.e. the basic blocks dominated by edge E and put them
   into array BBS, that will be allocated large enough to contain them.
   E->dest must have exactly one predecessor for this to work (it is
   easy to achieve and we do not put it here because we do not want to
   alter anything by this function).  The number of basic blocks in the
   path is returned.  */
static int
find_path (edge e, basic_block **bbs)
{
  gcc_assert (EDGE_COUNT (e->dest->preds) <= 1);

  /* Find bbs in the path.  */
  *bbs = XNEWVEC (basic_block, n_basic_blocks_for_fn (cfun));
  return dfs_enumerate_from (e->dest, 0, rpe_enum_p, *bbs,
			     n_basic_blocks_for_fn (cfun), e->dest);
}

/* Fix placement of basic block BB inside loop hierarchy --
   Let L be a loop to that BB belongs.  Then every successor of BB must either
     1) belong to some superloop of loop L, or
     2) be a header of loop K such that K->outer is superloop of L
   Returns true if we had to move BB into other loop to enforce this condition,
   false if the placement of BB was already correct (provided that placements
   of its successors are correct).  */
static bool
fix_bb_placement (basic_block bb)
{
  edge e;
  edge_iterator ei;
  class loop *loop = current_loops->tree_root, *act;

  FOR_EACH_EDGE (e, ei, bb->succs)
    {
      if (e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
	continue;

      act = e->dest->loop_father;
      if (act->header == e->dest)
	act = loop_outer (act);

      if (flow_loop_nested_p (loop, act))
	loop = act;
    }

  if (loop == bb->loop_father)
    return false;

  remove_bb_from_loops (bb);
  add_bb_to_loop (bb, loop);

  return true;
}

/* Fix placement of LOOP inside loop tree, i.e. find the innermost superloop
   of LOOP to that leads at least one exit edge of LOOP, and set it
   as the immediate superloop of LOOP.  Return true if the immediate superloop
   of LOOP changed.

   IRRED_INVALIDATED is set to true if a change in the loop structures might
   invalidate the information about irreducible regions.  */

static bool
fix_loop_placement (class loop *loop, bool *irred_invalidated,
		    bitmap loop_closed_ssa_invalidated)
{
  unsigned i;
  edge e;
  auto_vec<edge> exits = get_loop_exit_edges (loop);
  class loop *father = current_loops->tree_root, *act;
  bool ret = false;

  FOR_EACH_VEC_ELT (exits, i, e)
    {
      act = find_common_loop (loop, e->dest->loop_father);
      if (flow_loop_nested_p (father, act))
	father = act;
    }

  if (father != loop_outer (loop))
    {
      for (act = loop_outer (loop); act != father; act = loop_outer (act))
	act->num_nodes -= loop->num_nodes;
      flow_loop_tree_node_remove (loop);
      flow_loop_tree_node_add (father, loop);

      /* The exit edges of LOOP no longer exits its original immediate
	 superloops; remove them from the appropriate exit lists.  */
      FOR_EACH_VEC_ELT (exits, i, e)
	{
	  /* We may need to recompute irreducible loops.  */
	  if (e->flags & EDGE_IRREDUCIBLE_LOOP)
	    *irred_invalidated = true;
	  rescan_loop_exit (e, false, false);
	}
      /* Any LC SSA PHIs on e->dest might now be on the wrong edge
	 if their defs were in a former outer loop.  Also all uses
	 in the original inner loop of defs in the outer loop(s) now
	 require LC PHI nodes.  */
      if (loop_closed_ssa_invalidated)
	{
	  basic_block *bbs = get_loop_body (loop);
	  for (unsigned i = 0; i < loop->num_nodes; ++i)
	    bitmap_set_bit (loop_closed_ssa_invalidated, bbs[i]->index);
	  free (bbs);
	}

      ret = true;
    }

  return ret;
}

/* Fix placements of basic blocks inside loop hierarchy stored in loops; i.e.
   enforce condition stated in description of fix_bb_placement. We
   start from basic block FROM that had some of its successors removed, so that
   his placement no longer has to be correct, and iteratively fix placement of
   its predecessors that may change if placement of FROM changed.  Also fix
   placement of subloops of FROM->loop_father, that might also be altered due
   to this change; the condition for them is similar, except that instead of
   successors we consider edges coming out of the loops.

   If the changes may invalidate the information about irreducible regions,
   IRRED_INVALIDATED is set to true.

   If LOOP_CLOSED_SSA_INVLIDATED is non-zero then all basic blocks with
   changed loop_father are collected there. */

static void
fix_bb_placements (basic_block from,
		   bool *irred_invalidated,
		   bitmap loop_closed_ssa_invalidated)
{
  basic_block *queue, *qtop, *qbeg, *qend;
  class loop *base_loop, *target_loop;
  edge e;

  /* We pass through blocks back-reachable from FROM, testing whether some
     of their successors moved to outer loop.  It may be necessary to
     iterate several times, but it is finite, as we stop unless we move
     the basic block up the loop structure.  The whole story is a bit
     more complicated due to presence of subloops, those are moved using
     fix_loop_placement.  */

  base_loop = from->loop_father;
  /* If we are already in the outermost loop, the basic blocks cannot be moved
     outside of it.  If FROM is the header of the base loop, it cannot be moved
     outside of it, either.  In both cases, we can end now.  */
  if (base_loop == current_loops->tree_root
      || from == base_loop->header)
    return;

  auto_sbitmap in_queue (last_basic_block_for_fn (cfun));
  bitmap_clear (in_queue);
  bitmap_set_bit (in_queue, from->index);
  /* Prevent us from going out of the base_loop.  */
  bitmap_set_bit (in_queue, base_loop->header->index);

  queue = XNEWVEC (basic_block, base_loop->num_nodes + 1);
  qtop = queue + base_loop->num_nodes + 1;
  qbeg = queue;
  qend = queue + 1;
  *qbeg = from;

  while (qbeg != qend)
    {
      edge_iterator ei;
      from = *qbeg;
      qbeg++;
      if (qbeg == qtop)
	qbeg = queue;
      bitmap_clear_bit (in_queue, from->index);

      if (from->loop_father->header == from)
	{
	  /* Subloop header, maybe move the loop upward.  */
	  if (!fix_loop_placement (from->loop_father, irred_invalidated,
				   loop_closed_ssa_invalidated))
	    continue;
	  target_loop = loop_outer (from->loop_father);
	}
      else
	{
	  /* Ordinary basic block.  */
	  if (!fix_bb_placement (from))
	    continue;
	  target_loop = from->loop_father;
	  if (loop_closed_ssa_invalidated)
	    bitmap_set_bit (loop_closed_ssa_invalidated, from->index);
	}

      FOR_EACH_EDGE (e, ei, from->succs)
	{
	  if (e->flags & EDGE_IRREDUCIBLE_LOOP)
	    *irred_invalidated = true;
	}

      /* Something has changed, insert predecessors into queue.  */
      FOR_EACH_EDGE (e, ei, from->preds)
	{
	  basic_block pred = e->src;
	  class loop *nca;

	  if (e->flags & EDGE_IRREDUCIBLE_LOOP)
	    *irred_invalidated = true;

	  if (bitmap_bit_p (in_queue, pred->index))
	    continue;

	  /* If it is subloop, then it either was not moved, or
	     the path up the loop tree from base_loop do not contain
	     it.  */
	  nca = find_common_loop (pred->loop_father, base_loop);
	  if (pred->loop_father != base_loop
	      && (nca == base_loop
		  || nca != pred->loop_father))
	    pred = pred->loop_father->header;
	  else if (!flow_loop_nested_p (target_loop, pred->loop_father))
	    {
	      /* If PRED is already higher in the loop hierarchy than the
		 TARGET_LOOP to that we moved FROM, the change of the position
		 of FROM does not affect the position of PRED, so there is no
		 point in processing it.  */
	      continue;
	    }

	  if (bitmap_bit_p (in_queue, pred->index))
	    continue;

	  /* Schedule the basic block.  */
	  *qend = pred;
	  qend++;
	  if (qend == qtop)
	    qend = queue;
	  bitmap_set_bit (in_queue, pred->index);
	}
    }
  free (queue);
}

/* Removes path beginning at edge E, i.e. remove basic blocks dominated by E
   and update loop structures and dominators.  Return true if we were able
   to remove the path, false otherwise (and nothing is affected then).  */
bool
remove_path (edge e, bool *irred_invalidated,
	     bitmap loop_closed_ssa_invalidated)
{
  edge ae;
  basic_block *rem_bbs, *bord_bbs, from, bb;
  vec<basic_block> dom_bbs;
  int i, nrem, n_bord_bbs;
  bool local_irred_invalidated = false;
  edge_iterator ei;
  class loop *l, *f;

  if (! irred_invalidated)
    irred_invalidated = &local_irred_invalidated;

  if (!can_remove_branch_p (e))
    return false;

  /* Keep track of whether we need to update information about irreducible
     regions.  This is the case if the removed area is a part of the
     irreducible region, or if the set of basic blocks that belong to a loop
     that is inside an irreducible region is changed, or if such a loop is
     removed.  */
  if (e->flags & EDGE_IRREDUCIBLE_LOOP)
    *irred_invalidated = true;

  /* We need to check whether basic blocks are dominated by the edge
     e, but we only have basic block dominators.  This is easy to
     fix -- when e->dest has exactly one predecessor, this corresponds
     to blocks dominated by e->dest, if not, split the edge.  */
  if (!single_pred_p (e->dest))
    e = single_pred_edge (split_edge (e));

  /* It may happen that by removing path we remove one or more loops
     we belong to.  In this case first unloop the loops, then proceed
     normally.   We may assume that e->dest is not a header of any loop,
     as it now has exactly one predecessor.  */
  for (l = e->src->loop_father; loop_outer (l); l = f)
    {
      f = loop_outer (l);
      if (dominated_by_p (CDI_DOMINATORS, l->latch, e->dest))
        unloop (l, irred_invalidated, loop_closed_ssa_invalidated);
    }

  /* Identify the path.  */
  nrem = find_path (e, &rem_bbs);

  n_bord_bbs = 0;
  bord_bbs = XNEWVEC (basic_block, n_basic_blocks_for_fn (cfun));
  auto_sbitmap seen (last_basic_block_for_fn (cfun));
  bitmap_clear (seen);

  /* Find "border" hexes -- i.e. those with predecessor in removed path.  */
  for (i = 0; i < nrem; i++)
    bitmap_set_bit (seen, rem_bbs[i]->index);
  if (!*irred_invalidated)
    FOR_EACH_EDGE (ae, ei, e->src->succs)
      if (ae != e && ae->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)
	  && !bitmap_bit_p (seen, ae->dest->index)
	  && ae->flags & EDGE_IRREDUCIBLE_LOOP)
	{
	  *irred_invalidated = true;
	  break;
	}

  for (i = 0; i < nrem; i++)
    {
      FOR_EACH_EDGE (ae, ei, rem_bbs[i]->succs)
	if (ae->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)
	    && !bitmap_bit_p (seen, ae->dest->index))
	  {
	    bitmap_set_bit (seen, ae->dest->index);
	    bord_bbs[n_bord_bbs++] = ae->dest;

	    if (ae->flags & EDGE_IRREDUCIBLE_LOOP)
	      *irred_invalidated = true;
	  }
    }

  /* Remove the path.  */
  from = e->src;
  remove_branch (e);
  dom_bbs.create (0);

  /* Cancel loops contained in the path.  */
  for (i = 0; i < nrem; i++)
    if (rem_bbs[i]->loop_father->header == rem_bbs[i])
      cancel_loop_tree (rem_bbs[i]->loop_father);

  remove_bbs (rem_bbs, nrem);
  free (rem_bbs);

  /* Find blocks whose dominators may be affected.  */
  bitmap_clear (seen);
  for (i = 0; i < n_bord_bbs; i++)
    {
      basic_block ldom;

      bb = get_immediate_dominator (CDI_DOMINATORS, bord_bbs[i]);
      if (bitmap_bit_p (seen, bb->index))
	continue;
      bitmap_set_bit (seen, bb->index);

      for (ldom = first_dom_son (CDI_DOMINATORS, bb);
	   ldom;
	   ldom = next_dom_son (CDI_DOMINATORS, ldom))
	if (!dominated_by_p (CDI_DOMINATORS, from, ldom))
	  dom_bbs.safe_push (ldom);
    }

  /* Recount dominators.  */
  iterate_fix_dominators (CDI_DOMINATORS, dom_bbs, true);
  dom_bbs.release ();
  free (bord_bbs);

  /* Fix placements of basic blocks inside loops and the placement of
     loops in the loop tree.  */
  fix_bb_placements (from, irred_invalidated, loop_closed_ssa_invalidated);
  fix_loop_placements (from->loop_father, irred_invalidated,
		       loop_closed_ssa_invalidated);

  if (local_irred_invalidated
      && loops_state_satisfies_p (LOOPS_HAVE_MARKED_IRREDUCIBLE_REGIONS))
    mark_irreducible_loops ();

  return true;
}

/* Creates place for a new LOOP in loops structure of FN.  */

void
place_new_loop (struct function *fn, class loop *loop)
{
  loop->num = number_of_loops (fn);
  vec_safe_push (loops_for_fn (fn)->larray, loop);
}

/* Given LOOP structure with filled header and latch, find the body of the
   corresponding loop and add it to loops tree.  Insert the LOOP as a son of
   outer.  */

void
add_loop (class loop *loop, class loop *outer)
{
  basic_block *bbs;
  int i, n;
  class loop *subloop;
  edge e;
  edge_iterator ei;

  /* Add it to loop structure.  */
  place_new_loop (cfun, loop);
  flow_loop_tree_node_add (outer, loop);

  /* Find its nodes.  */
  bbs = XNEWVEC (basic_block, n_basic_blocks_for_fn (cfun));
  n = get_loop_body_with_size (loop, bbs, n_basic_blocks_for_fn (cfun));

  for (i = 0; i < n; i++)
    {
      if (bbs[i]->loop_father == outer)
	{
	  remove_bb_from_loops (bbs[i]);
	  add_bb_to_loop (bbs[i], loop);
	  continue;
	}

      loop->num_nodes++;

      /* If we find a direct subloop of OUTER, move it to LOOP.  */
      subloop = bbs[i]->loop_father;
      if (loop_outer (subloop) == outer
	  && subloop->header == bbs[i])
	{
	  flow_loop_tree_node_remove (subloop);
	  flow_loop_tree_node_add (loop, subloop);
	}
    }

  /* Update the information about loop exit edges.  */
  for (i = 0; i < n; i++)
    {
      FOR_EACH_EDGE (e, ei, bbs[i]->succs)
	{
	  rescan_loop_exit (e, false, false);
	}
    }

  free (bbs);
}

/* Scale profile of loop by P.  */

void
scale_loop_frequencies (class loop *loop, profile_probability p)
{
  basic_block *bbs;

  bbs = get_loop_body (loop);
  scale_bbs_frequencies (bbs, loop->num_nodes, p);
  free (bbs);
}

/* Scales the frequencies of all basic blocks in LOOP that are strictly
   dominated by BB by NUM/DEN.  */

void
scale_dominated_blocks_in_loop (class loop *loop, basic_block bb,
				profile_count num, profile_count den)
{
  basic_block son;

  if (!den.nonzero_p () && !(num == profile_count::zero ()))
    return;
  auto_vec <basic_block, 8> worklist;
  worklist.safe_push (bb);

  while (!worklist.is_empty ())
    for (son = first_dom_son (CDI_DOMINATORS, worklist.pop ());
	 son;
	 son = next_dom_son (CDI_DOMINATORS, son))
      {
	if (!flow_bb_inside_loop_p (loop, son))
	  continue;
	son->count = son->count.apply_scale (num, den);
	worklist.safe_push (son);
      }
}

/* Return exit that suitable for update when loop iterations
   changed.  */

static edge
loop_exit_for_scaling (class loop *loop)
{
  edge exit_edge = single_exit (loop);
  if (!exit_edge)
    {
      auto_vec<edge> exits = get_loop_exit_edges  (loop);
      exit_edge = single_likely_exit (loop, exits);
    }
  return exit_edge;
}

/* Assume that loop's entry count and profile up to a given EXIT_EDGE is
   consistent. Update exit probability so loop exists with PROFILE_COUNT
   and rescale profile of basic blocks inside loop dominated by EXIT_EDGE->src.

   This is useful after number of iteraitons of loop has changed.
   If EXIT_EDGE is NULL, the function will try to identify suitable exit.
   If DESIRED_COUNT is NULL, loop entry count will be used.
   In consistent profile usually loop exists as many times as it is entred.

   Return updated exit if successfull and NULL otherwise. */

edge
update_loop_exit_probability_scale_dom_bbs (class loop *loop,
					    edge exit_edge,
					    profile_count desired_count)
{
  if (!exit_edge)
    exit_edge = loop_exit_for_scaling (loop);
  if (!exit_edge)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, ";; Not updating exit probability of loop %i;"
		   " it has no single exit\n",
		   loop->num);
	}
      return NULL;
    }
  /* If exit is inside another loop, adjusting its probability will also
     adjust number of the inner loop iterations.  Just do noting for now.  */
  if (!just_once_each_iteration_p (loop, exit_edge->src))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, ";; Not updating exit probability of loop %i;"
		   " exit is inside inner loop\n",
		   loop->num);
	}
      return NULL;
    }
  /* Normal loops exit as many times as they are entered.  */
  if (!desired_count.initialized_p ())
    desired_count = loop_count_in (loop);
  /* See if everything is already perfect.  */
  if (desired_count == exit_edge->count ())
    return exit_edge;
  profile_count old_exit_count = exit_edge->count ();
  /* See if update is possible.
     Avoid turning probability to 0 or 1 just trying to reach impossible
     value.

     Natural profile update after some loop will happily scale header count to
     be less than count of entering the loop.  This is bad idea and they should
     special case maybe_flat_loop_profile.

     It may also happen that the source basic block of the exit edge is
     inside in-loop condition:

	  +-> header
	  |    |
	  |   B1
	  |  /  \
	  | |   B2--exit_edge-->
	  |  \  /
	  |   B3
	  +__/

      If B2 count is smaller than desired exit edge count
      the profile was inconsistent with the newly discovered upper bound.
      Probablity of edge B1->B2 is too low.  We do not attempt to fix
      that (as it is hard in general).  */
  if (desired_count > exit_edge->src->count
      && exit_edge->src->count.differs_from_p (desired_count))
    {
      if (dump_file)
	{
	  fprintf (dump_file, ";; Source bb of loop %i has count ",
		   loop->num);
	  exit_edge->src->count.dump (dump_file, cfun);
	  fprintf (dump_file,
		   " which is smaller then desired count of exitting loop ");
	  desired_count.dump (dump_file, cfun);
	  fprintf (dump_file, ". Profile update is impossible.\n");
	}
      /* Drop quality of probability since we know it likely
	 bad.  */
      exit_edge->probability = exit_edge->probability.guessed ();
      return NULL;
    }
  if (!exit_edge->src->count.nonzero_p ())
    {
      if (dump_file)
	{
	  fprintf (dump_file, ";; Not updating exit edge probability"
		   " in loop %i since profile is zero ",
		   loop->num);
	}
      return NULL;
    }
  set_edge_probability_and_rescale_others
    (exit_edge, desired_count.probability_in (exit_edge->src->count));
  /* Rescale the remaining edge probabilities and see if there is only
     one.  */
  edge other_edge = NULL;
  bool found = false;
  edge e;
  edge_iterator ei;
  FOR_EACH_EDGE (e, ei, exit_edge->src->succs)
    if (!(e->flags & EDGE_FAKE)
	&& !loop_exit_edge_p (loop, e))
      {
	if (found)
	  {
	    other_edge = NULL;
	    break;
	  }
	other_edge = e;
	found = true;
      }
  /* If there is only loop latch after other edge,
     update its profile.  This is tiny bit more precise
     than scaling.  */
  if (other_edge && other_edge->dest == loop->latch)
    {
      if (single_pred_p (loop->latch))
	loop->latch->count = loop->latch->count
			     + old_exit_count - exit_edge->count ();
    }
  else
    /* If there are multiple blocks, just scale.  */
    scale_dominated_blocks_in_loop (loop, exit_edge->src,
				    exit_edge->src->count - exit_edge->count (),
				    exit_edge->src->count - old_exit_count);
  return exit_edge;
}

/* Scale profile in LOOP by P.
   If ITERATION_BOUND is not -1, scale even further if loop is predicted
   to iterate too many times.
   Before caling this function, preheader block profile should be already
   scaled to final count.  This is necessary because loop iterations are
   determined by comparing header edge count to latch ege count and thus
   they need to be scaled synchronously.  */

void
scale_loop_profile (class loop *loop, profile_probability p,
		    gcov_type iteration_bound)
{
  if (!(p == profile_probability::always ()))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, ";; Scaling loop %i with scale ",
		   loop->num);
	  p.dump (dump_file);
	  fprintf (dump_file, "\n");
	}

      /* Scale the probabilities.  */
      scale_loop_frequencies (loop, p);
    }

  if (iteration_bound == -1)
    return;

  sreal iterations;
  if (!expected_loop_iterations_by_profile (loop, &iterations))
    return;

  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file,
	       ";; Guessed iterations of loop %i is %f. New upper bound %i.\n",
	       loop->num,
	       iterations.to_double (),
	       (int)iteration_bound);
    }

  /* See if loop is predicted to iterate too many times.  */
  if (iterations <= (sreal)iteration_bound)
    return;

  profile_count count_in = loop_count_in (loop);

  /* Now scale the loop body so header count is
     count_in * (iteration_bound + 1)  */
  profile_probability scale_prob
    = (count_in * (iteration_bound + 1)).probability_in (loop->header->count);
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, ";; Scaling loop %i with scale ",
	       loop->num);
      scale_prob.dump (dump_file);
      fprintf (dump_file, " to reach upper bound %i\n",
	       (int)iteration_bound);
    }
  /* Finally attempt to fix exit edge probability.  */
  edge exit_edge = loop_exit_for_scaling (loop);

  /* In a consistent profile unadjusted_exit_count should be same as count_in,
     however to preserve as much of the original info, avoid recomputing
     it.  */
  profile_count unadjusted_exit_count = profile_count::uninitialized ();
  if (exit_edge)
    unadjusted_exit_count = exit_edge->count ();
  scale_loop_frequencies (loop, scale_prob);
  update_loop_exit_probability_scale_dom_bbs (loop, exit_edge,
					      unadjusted_exit_count);
}

/* Recompute dominance information for basic blocks outside LOOP.  */

static void
update_dominators_in_loop (class loop *loop)
{
  vec<basic_block> dom_bbs = vNULL;
  basic_block *body;
  unsigned i;

  auto_sbitmap seen (last_basic_block_for_fn (cfun));
  bitmap_clear (seen);
  body = get_loop_body (loop);

  for (i = 0; i < loop->num_nodes; i++)
    bitmap_set_bit (seen, body[i]->index);

  for (i = 0; i < loop->num_nodes; i++)
    {
      basic_block ldom;

      for (ldom = first_dom_son (CDI_DOMINATORS, body[i]);
	   ldom;
	   ldom = next_dom_son (CDI_DOMINATORS, ldom))
	if (!bitmap_bit_p (seen, ldom->index))
	  {
	    bitmap_set_bit (seen, ldom->index);
	    dom_bbs.safe_push (ldom);
	  }
    }

  iterate_fix_dominators (CDI_DOMINATORS, dom_bbs, false);
  free (body);
  dom_bbs.release ();
}

/* Creates an if region as shown above. CONDITION is used to create
   the test for the if.

   |
   |     -------------                 -------------
   |     |  pred_bb  |                 |  pred_bb  |
   |     -------------                 -------------
   |           |                             |
   |           |                             | ENTRY_EDGE
   |           | ENTRY_EDGE                  V
   |           |             ====>     -------------
   |           |                       |  cond_bb  |
   |           |                       | CONDITION |
   |           |                       -------------
   |           V                        /         \
   |     -------------         e_false /           \ e_true
   |     |  succ_bb  |                V             V
   |     -------------         -----------       -----------
   |                           | false_bb |      | true_bb |
   |                           -----------       -----------
   |                                   \           /
   |                                    \         /
   |                                     V       V
   |                                   -------------
   |                                   |  join_bb  |
   |                                   -------------
   |                                         | exit_edge (result)
   |                                         V
   |                                    -----------
   |                                    | succ_bb |
   |                                    -----------
   |
 */

edge
create_empty_if_region_on_edge (edge entry_edge, tree condition)
{

  basic_block cond_bb, true_bb, false_bb, join_bb;
  edge e_true, e_false, exit_edge;
  gcond *cond_stmt;
  tree simple_cond;
  gimple_stmt_iterator gsi;

  cond_bb = split_edge (entry_edge);

  /* Insert condition in cond_bb.  */
  gsi = gsi_last_bb (cond_bb);
  simple_cond =
    force_gimple_operand_gsi (&gsi, condition, true, NULL,
			      false, GSI_NEW_STMT);
  cond_stmt = gimple_build_cond_from_tree (simple_cond, NULL_TREE, NULL_TREE);
  gsi = gsi_last_bb (cond_bb);
  gsi_insert_after (&gsi, cond_stmt, GSI_NEW_STMT);

  join_bb = split_edge (single_succ_edge (cond_bb));

  e_true = single_succ_edge (cond_bb);
  true_bb = split_edge (e_true);

  e_false = make_edge (cond_bb, join_bb, 0);
  false_bb = split_edge (e_false);

  e_true->flags &= ~EDGE_FALLTHRU;
  e_true->flags |= EDGE_TRUE_VALUE;
  e_false->flags &= ~EDGE_FALLTHRU;
  e_false->flags |= EDGE_FALSE_VALUE;

  set_immediate_dominator (CDI_DOMINATORS, cond_bb, entry_edge->src);
  set_immediate_dominator (CDI_DOMINATORS, true_bb, cond_bb);
  set_immediate_dominator (CDI_DOMINATORS, false_bb, cond_bb);
  set_immediate_dominator (CDI_DOMINATORS, join_bb, cond_bb);

  exit_edge = single_succ_edge (join_bb);

  if (single_pred_p (exit_edge->dest))
    set_immediate_dominator (CDI_DOMINATORS, exit_edge->dest, join_bb);

  return exit_edge;
}

/* create_empty_loop_on_edge
   |
   |    - pred_bb -                   ------ pred_bb ------
   |   |           |                 | iv0 = initial_value |
   |    -----|-----                   ---------|-----------
   |         |                       ______    | entry_edge
   |         | entry_edge           /      |   |
   |         |             ====>   |      -V---V- loop_header -------------
   |         V                     |     | iv_before = phi (iv0, iv_after) |
   |    - succ_bb -                |      ---|-----------------------------
   |   |           |               |         |
   |    -----------                |      ---V--- loop_body ---------------
   |                               |     | iv_after = iv_before + stride   |
   |                               |     | if (iv_before < upper_bound)    |
   |                               |      ---|--------------\--------------
   |                               |         |               \ exit_e
   |                               |         V                \
   |                               |       - loop_latch -      V- succ_bb -
   |                               |      |              |     |           |
   |                               |       /-------------       -----------
   |                                \ ___ /

   Creates an empty loop as shown above, the IV_BEFORE is the SSA_NAME
   that is used before the increment of IV. IV_BEFORE should be used for
   adding code to the body that uses the IV.  OUTER is the outer loop in
   which the new loop should be inserted.

   Both INITIAL_VALUE and UPPER_BOUND expressions are gimplified and
   inserted on the loop entry edge.  This implies that this function
   should be used only when the UPPER_BOUND expression is a loop
   invariant.  */

class loop *
create_empty_loop_on_edge (edge entry_edge,
			   tree initial_value,
			   tree stride, tree upper_bound,
			   tree iv,
			   tree *iv_before,
			   tree *iv_after,
			   class loop *outer)
{
  basic_block loop_header, loop_latch, succ_bb, pred_bb;
  class loop *loop;
  gimple_stmt_iterator gsi;
  gimple_seq stmts;
  gcond *cond_expr;
  tree exit_test;
  edge exit_e;

  gcc_assert (entry_edge && initial_value && stride && upper_bound && iv);

  /* Create header, latch and wire up the loop.  */
  pred_bb = entry_edge->src;
  loop_header = split_edge (entry_edge);
  loop_latch = split_edge (single_succ_edge (loop_header));
  succ_bb = single_succ (loop_latch);
  make_edge (loop_header, succ_bb, 0);
  redirect_edge_succ_nodup (single_succ_edge (loop_latch), loop_header);

  /* Set immediate dominator information.  */
  set_immediate_dominator (CDI_DOMINATORS, loop_header, pred_bb);
  set_immediate_dominator (CDI_DOMINATORS, loop_latch, loop_header);
  set_immediate_dominator (CDI_DOMINATORS, succ_bb, loop_header);

  /* Initialize a loop structure and put it in a loop hierarchy.  */
  loop = alloc_loop ();
  loop->header = loop_header;
  loop->latch = loop_latch;
  add_loop (loop, outer);

  /* TODO: Fix counts.  */
  scale_loop_frequencies (loop, profile_probability::even ());

  /* Update dominators.  */
  update_dominators_in_loop (loop);

  /* Modify edge flags.  */
  exit_e = single_exit (loop);
  exit_e->flags = EDGE_LOOP_EXIT | EDGE_FALSE_VALUE;
  single_pred_edge (loop_latch)->flags = EDGE_TRUE_VALUE;

  /* Construct IV code in loop.  */
  initial_value = force_gimple_operand (initial_value, &stmts, true, iv);
  if (stmts)
    {
      gsi_insert_seq_on_edge (loop_preheader_edge (loop), stmts);
      gsi_commit_edge_inserts ();
    }

  upper_bound = force_gimple_operand (upper_bound, &stmts, true, NULL);
  if (stmts)
    {
      gsi_insert_seq_on_edge (loop_preheader_edge (loop), stmts);
      gsi_commit_edge_inserts ();
    }

  gsi = gsi_last_bb (loop_header);
  create_iv (initial_value, PLUS_EXPR, stride, iv, loop, &gsi, false,
	     iv_before, iv_after);

  /* Insert loop exit condition.  */
  cond_expr = gimple_build_cond
    (LT_EXPR, *iv_before, upper_bound, NULL_TREE, NULL_TREE);

  exit_test = gimple_cond_lhs (cond_expr);
  exit_test = force_gimple_operand_gsi (&gsi, exit_test, true, NULL,
					false, GSI_NEW_STMT);
  gimple_cond_set_lhs (cond_expr, exit_test);
  gsi = gsi_last_bb (exit_e->src);
  gsi_insert_after (&gsi, cond_expr, GSI_NEW_STMT);

  split_block_after_labels (loop_header);

  return loop;
}

/* Remove the latch edge of a LOOP and update loops to indicate that
   the LOOP was removed.  After this function, original loop latch will
   have no successor, which caller is expected to fix somehow.

   If this may cause the information about irreducible regions to become
   invalid, IRRED_INVALIDATED is set to true.

   LOOP_CLOSED_SSA_INVALIDATED, if non-NULL, is a bitmap where we store
   basic blocks that had non-trivial update on their loop_father.*/

void
unloop (class loop *loop, bool *irred_invalidated,
	bitmap loop_closed_ssa_invalidated)
{
  basic_block *body;
  class loop *ploop;
  unsigned i, n;
  basic_block latch = loop->latch;
  bool dummy = false;

  if (loop_preheader_edge (loop)->flags & EDGE_IRREDUCIBLE_LOOP)
    *irred_invalidated = true;

  /* This is relatively straightforward.  The dominators are unchanged, as
     loop header dominates loop latch, so the only thing we have to care of
     is the placement of loops and basic blocks inside the loop tree.  We
     move them all to the loop->outer, and then let fix_bb_placements do
     its work.  */

  body = get_loop_body (loop);
  n = loop->num_nodes;
  for (i = 0; i < n; i++)
    if (body[i]->loop_father == loop)
      {
	remove_bb_from_loops (body[i]);
	add_bb_to_loop (body[i], loop_outer (loop));
      }
  free (body);

  while (loop->inner)
    {
      ploop = loop->inner;
      flow_loop_tree_node_remove (ploop);
      flow_loop_tree_node_add (loop_outer (loop), ploop);
    }

  /* Remove the loop and free its data.  */
  delete_loop (loop);

  remove_edge (single_succ_edge (latch));

  /* We do not pass IRRED_INVALIDATED to fix_bb_placements here, as even if
     there is an irreducible region inside the cancelled loop, the flags will
     be still correct.  */
  fix_bb_placements (latch, &dummy, loop_closed_ssa_invalidated);
}

/* Fix placement of superloops of LOOP inside loop tree, i.e. ensure that
   condition stated in description of fix_loop_placement holds for them.
   It is used in case when we removed some edges coming out of LOOP, which
   may cause the right placement of LOOP inside loop tree to change.

   IRRED_INVALIDATED is set to true if a change in the loop structures might
   invalidate the information about irreducible regions.  */

static void
fix_loop_placements (class loop *loop, bool *irred_invalidated,
		     bitmap loop_closed_ssa_invalidated)
{
  class loop *outer;

  while (loop_outer (loop))
    {
      outer = loop_outer (loop);
      if (!fix_loop_placement (loop, irred_invalidated,
			       loop_closed_ssa_invalidated))
	break;

      /* Changing the placement of a loop in the loop tree may alter the
	 validity of condition 2) of the description of fix_bb_placement
	 for its preheader, because the successor is the header and belongs
	 to the loop.  So call fix_bb_placements to fix up the placement
	 of the preheader and (possibly) of its predecessors.  */
      fix_bb_placements (loop_preheader_edge (loop)->src,
			 irred_invalidated, loop_closed_ssa_invalidated);
      loop = outer;
    }
}

/* Duplicate loop bounds and other information we store about
   the loop into its duplicate.  */

void
copy_loop_info (class loop *loop, class loop *target)
{
  gcc_checking_assert (!target->any_upper_bound && !target->any_estimate);
  target->any_upper_bound = loop->any_upper_bound;
  target->nb_iterations_upper_bound = loop->nb_iterations_upper_bound;
  target->any_likely_upper_bound = loop->any_likely_upper_bound;
  target->nb_iterations_likely_upper_bound
    = loop->nb_iterations_likely_upper_bound;
  target->any_estimate = loop->any_estimate;
  target->nb_iterations_estimate = loop->nb_iterations_estimate;
  target->estimate_state = loop->estimate_state;
  target->safelen = loop->safelen;
  target->simdlen = loop->simdlen;
  target->constraints = loop->constraints;
  target->can_be_parallel = loop->can_be_parallel;
  target->warned_aggressive_loop_optimizations
    |= loop->warned_aggressive_loop_optimizations;
  target->dont_vectorize = loop->dont_vectorize;
  target->force_vectorize = loop->force_vectorize;
  target->in_oacc_kernels_region = loop->in_oacc_kernels_region;
  target->finite_p = loop->finite_p;
  target->unroll = loop->unroll;
  target->owned_clique = loop->owned_clique;
}

/* Copies copy of LOOP as subloop of TARGET loop, placing newly
   created loop into loops structure.  If AFTER is non-null
   the new loop is added at AFTER->next, otherwise in front of TARGETs
   sibling list.  */
class loop *
duplicate_loop (class loop *loop, class loop *target, class loop *after)
{
  class loop *cloop;
  cloop = alloc_loop ();
  place_new_loop (cfun, cloop);

  copy_loop_info (loop, cloop);

  /* Mark the new loop as copy of LOOP.  */
  set_loop_copy (loop, cloop);

  /* Add it to target.  */
  flow_loop_tree_node_add (target, cloop, after);

  return cloop;
}

/* Copies structure of subloops of LOOP into TARGET loop, placing
   newly created loops into loop tree at the end of TARGETs sibling
   list in the original order.  */
void
duplicate_subloops (class loop *loop, class loop *target)
{
  class loop *aloop, *cloop, *tail;

  for (tail = target->inner; tail && tail->next; tail = tail->next)
    ;
  for (aloop = loop->inner; aloop; aloop = aloop->next)
    {
      cloop = duplicate_loop (aloop, target, tail);
      tail = cloop;
      gcc_assert(!tail->next);
      duplicate_subloops (aloop, cloop);
    }
}

/* Copies structure of subloops of N loops, stored in array COPIED_LOOPS,
   into TARGET loop, placing newly created loops into loop tree adding
   them to TARGETs sibling list at the end in order.  */
static void
copy_loops_to (class loop **copied_loops, int n, class loop *target)
{
  class loop *aloop, *tail;
  int i;

  for (tail = target->inner; tail && tail->next; tail = tail->next)
    ;
  for (i = 0; i < n; i++)
    {
      aloop = duplicate_loop (copied_loops[i], target, tail);
      tail = aloop;
      gcc_assert(!tail->next);
      duplicate_subloops (copied_loops[i], aloop);
    }
}

/* Redirects edge E to basic block DEST.  */
static void
loop_redirect_edge (edge e, basic_block dest)
{
  if (e->dest == dest)
    return;

  redirect_edge_and_branch_force (e, dest);
}

/* Check whether LOOP's body can be duplicated.  */
bool
can_duplicate_loop_p (const class loop *loop)
{
  int ret;
  basic_block *bbs = get_loop_body (loop);

  ret = can_copy_bbs_p (bbs, loop->num_nodes);
  free (bbs);

  return ret;
}

/* Duplicates body of LOOP to given edge E NDUPL times.  Takes care of updating
   loop structure and dominators (order of inner subloops is retained).
   E's destination must be LOOP header for this to work, i.e. it must be entry
   or latch edge of this loop; these are unique, as the loops must have
   preheaders for this function to work correctly (in case E is latch, the
   function unrolls the loop, if E is entry edge, it peels the loop).  Store
   edges created by copying ORIG edge from copies corresponding to set bits in
   WONT_EXIT bitmap (bit 0 corresponds to original LOOP body, the other copies
   are numbered in order given by control flow through them) into TO_REMOVE
   array.  Returns false if duplication is
   impossible.  */

bool
duplicate_loop_body_to_header_edge (class loop *loop, edge e,
				    unsigned int ndupl, sbitmap wont_exit,
				    edge orig, vec<edge> *to_remove, int flags)
{
  class loop *target, *aloop;
  class loop **orig_loops;
  unsigned n_orig_loops;
  basic_block header = loop->header, latch = loop->latch;
  basic_block *new_bbs, *bbs, *first_active;
  basic_block new_bb, bb, first_active_latch = NULL;
  edge ae, latch_edge;
  edge spec_edges[2], new_spec_edges[2];
  const int SE_LATCH = 0;
  const int SE_ORIG = 1;
  unsigned i, j, n;
  int is_latch = (latch == e->src);
  profile_probability *scale_step = NULL;
  profile_probability scale_main = profile_probability::always ();
  profile_probability scale_act = profile_probability::always ();
  profile_count after_exit_num = profile_count::zero (),
	        after_exit_den = profile_count::zero ();
  bool scale_after_exit = false;
  int add_irreducible_flag;
  basic_block place_after;
  bitmap bbs_to_scale = NULL;
  bitmap_iterator bi;

  gcc_assert (e->dest == loop->header);
  gcc_assert (ndupl > 0);

  if (orig)
    {
      /* Orig must be edge out of the loop.  */
      gcc_assert (flow_bb_inside_loop_p (loop, orig->src));
      gcc_assert (!flow_bb_inside_loop_p (loop, orig->dest));
    }

  n = loop->num_nodes;
  bbs = get_loop_body_in_dom_order (loop);
  gcc_assert (bbs[0] == loop->header);
  gcc_assert (bbs[n  - 1] == loop->latch);

  /* Check whether duplication is possible.  */
  if (!can_copy_bbs_p (bbs, loop->num_nodes))
    {
      free (bbs);
      return false;
    }
  new_bbs = XNEWVEC (basic_block, loop->num_nodes);

  /* In case we are doing loop peeling and the loop is in the middle of
     irreducible region, the peeled copies will be inside it too.  */
  add_irreducible_flag = e->flags & EDGE_IRREDUCIBLE_LOOP;
  gcc_assert (!is_latch || !add_irreducible_flag);

  /* Find edge from latch.  */
  latch_edge = loop_latch_edge (loop);

  if (flags & DLTHE_FLAG_UPDATE_FREQ)
    {
      /* Calculate coefficients by that we have to scale counts
	 of duplicated loop bodies.  */
      profile_count count_in = header->count;
      profile_count count_le = latch_edge->count ();
      profile_count count_out_orig = orig ? orig->count () : count_in - count_le;
      profile_probability prob_pass_thru = count_le.probability_in (count_in);
      profile_count new_count_le = count_le + count_out_orig;

      if (orig && orig->probability.initialized_p ()
	  && !(orig->probability == profile_probability::always ()))
	{
	  /* The blocks that are dominated by a removed exit edge ORIG have
	     frequencies scaled by this.  */
	  if (orig->count ().initialized_p ())
	    {
	      after_exit_num = orig->src->count;
	      after_exit_den = after_exit_num - orig->count ();
	      scale_after_exit = true;
	    }
	  bbs_to_scale = BITMAP_ALLOC (NULL);
	  for (i = 0; i < n; i++)
	    {
	      if (bbs[i] != orig->src
		  && dominated_by_p (CDI_DOMINATORS, bbs[i], orig->src))
		bitmap_set_bit (bbs_to_scale, i);
	    }
	  /* Since we will scale up all basic blocks dominated by orig, exits
	     will become more likely; compensate for that.  */
	  if (after_exit_den.nonzero_p ())
	    {
	      auto_vec<edge> exits = get_loop_exit_edges (loop);
	      for (edge ex : exits)
		if (ex != orig
		    && dominated_by_p (CDI_DOMINATORS, ex->src, orig->src))
		  new_count_le -= ex->count ().apply_scale (after_exit_num
							    - after_exit_den,
							    after_exit_den);
	    }
	}
      profile_probability prob_pass_wont_exit =
	      new_count_le.probability_in (count_in);
      /* If profile count is 0, the probability will be uninitialized.
	 We can set probability to any initialized value to avoid
	 precision loss.  If profile is sane, all counts will be 0 anyway.  */
      if (!count_in.nonzero_p ())
	{
	  prob_pass_thru
		  = profile_probability::always ().apply_scale (1, 2);
	  prob_pass_wont_exit
		  = profile_probability::always ().apply_scale (1, 2);
	}

      scale_step = XNEWVEC (profile_probability, ndupl);

      for (i = 1; i <= ndupl; i++)
	scale_step[i - 1] = bitmap_bit_p (wont_exit, i)
				? prob_pass_wont_exit
				: prob_pass_thru;

      /* Complete peeling is special as the probability of exit in last
	 copy becomes 1.  */
      if (!count_in.nonzero_p ())
	;
      else if (flags & DLTHE_FLAG_COMPLETTE_PEEL)
	{
	  profile_count wanted_count = e->count ();

	  gcc_assert (!is_latch);
	  /* First copy has count of incoming edge.  Each subsequent
	     count should be reduced by prob_pass_wont_exit.  Caller
	     should've managed the flags so all except for original loop
	     has won't exist set.  */
	  scale_act = wanted_count.probability_in (count_in);

	  /* Now simulate the duplication adjustments and compute header
	     frequency of the last copy.  */
	  for (i = 0; i < ndupl; i++)
	    wanted_count = wanted_count.apply_probability (scale_step [i]);
	  scale_main = wanted_count.probability_in (count_in);
	}
      /* Here we insert loop bodies inside the loop itself (for loop unrolling).
	 First iteration will be original loop followed by duplicated bodies.
	 It is necessary to scale down the original so we get right overall
	 number of iterations.  */
      else if (is_latch)
	{
	  profile_probability prob_pass_main = bitmap_bit_p (wont_exit, 0)
							? prob_pass_wont_exit
							: prob_pass_thru;
	  if (!(flags & DLTHE_FLAG_FLAT_PROFILE))
	    {
	      profile_probability p = prob_pass_main;
	      profile_count scale_main_den = count_in;
	      for (i = 0; i < ndupl; i++)
		{
		  scale_main_den += count_in.apply_probability (p);
		  p = p * scale_step[i];
		}
	      /* If original loop is executed COUNT_IN times, the unrolled
		 loop will account SCALE_MAIN_DEN times.  */
	      scale_main = count_in.probability_in (scale_main_den);
	    }
	  else
	    scale_main = profile_probability::always ();
	  scale_act = scale_main * prob_pass_main;
	}
      else
	{
	  profile_count preheader_count = e->count ();
	  for (i = 0; i < ndupl; i++)
	    scale_main = scale_main * scale_step[i];
	  scale_act = preheader_count.probability_in (count_in);
	}
    }

  /* Loop the new bbs will belong to.  */
  target = e->src->loop_father;

  /* Original loops.  */
  n_orig_loops = 0;
  for (aloop = loop->inner; aloop; aloop = aloop->next)
    n_orig_loops++;
  orig_loops = XNEWVEC (class loop *, n_orig_loops);
  for (aloop = loop->inner, i = 0; aloop; aloop = aloop->next, i++)
    orig_loops[i] = aloop;

  set_loop_copy (loop, target);

  first_active = XNEWVEC (basic_block, n);
  if (is_latch)
    {
      memcpy (first_active, bbs, n * sizeof (basic_block));
      first_active_latch = latch;
    }

  spec_edges[SE_ORIG] = orig;
  spec_edges[SE_LATCH] = latch_edge;

  place_after = e->src;
  for (j = 0; j < ndupl; j++)
    {
      /* Copy loops.  */
      copy_loops_to (orig_loops, n_orig_loops, target);

      /* Copy bbs.  */
      copy_bbs (bbs, n, new_bbs, spec_edges, 2, new_spec_edges, loop,
		place_after, true);
      place_after = new_spec_edges[SE_LATCH]->src;

      if (flags & DLTHE_RECORD_COPY_NUMBER)
	for (i = 0; i < n; i++)
	  {
	    gcc_assert (!new_bbs[i]->aux);
	    new_bbs[i]->aux = (void *)(size_t)(j + 1);
	  }

      /* Note whether the blocks and edges belong to an irreducible loop.  */
      if (add_irreducible_flag)
	{
	  for (i = 0; i < n; i++)
	    new_bbs[i]->flags |= BB_DUPLICATED;
	  for (i = 0; i < n; i++)
	    {
	      edge_iterator ei;
	      new_bb = new_bbs[i];
	      if (new_bb->loop_father == target)
		new_bb->flags |= BB_IRREDUCIBLE_LOOP;

	      FOR_EACH_EDGE (ae, ei, new_bb->succs)
		if ((ae->dest->flags & BB_DUPLICATED)
		    && (ae->src->loop_father == target
			|| ae->dest->loop_father == target))
		  ae->flags |= EDGE_IRREDUCIBLE_LOOP;
	    }
	  for (i = 0; i < n; i++)
	    new_bbs[i]->flags &= ~BB_DUPLICATED;
	}

      /* Redirect the special edges.  */
      if (is_latch)
	{
	  redirect_edge_and_branch_force (latch_edge, new_bbs[0]);
	  redirect_edge_and_branch_force (new_spec_edges[SE_LATCH],
					  loop->header);
	  set_immediate_dominator (CDI_DOMINATORS, new_bbs[0], latch);
	  latch = loop->latch = new_bbs[n - 1];
	  e = latch_edge = new_spec_edges[SE_LATCH];
	}
      else
	{
	  redirect_edge_and_branch_force (e, new_bbs[0]);
	  redirect_edge_and_branch_force (new_spec_edges[SE_LATCH],
					  loop->header);
	  set_immediate_dominator (CDI_DOMINATORS, new_bbs[0], e->src);
	  e = new_spec_edges[SE_LATCH];
	}

      /* Record exit edge in this copy.  */
      if (orig && bitmap_bit_p (wont_exit, j + 1))
	{
	  if (to_remove)
	    to_remove->safe_push (new_spec_edges[SE_ORIG]);
	  force_edge_cold (new_spec_edges[SE_ORIG], true);

	  /* Scale the frequencies of the blocks dominated by the exit.  */
	  if (bbs_to_scale && scale_after_exit)
	    {
	      EXECUTE_IF_SET_IN_BITMAP (bbs_to_scale, 0, i, bi)
		scale_bbs_frequencies_profile_count (new_bbs + i, 1, after_exit_num,
						     after_exit_den);
	    }
	}

      /* Record the first copy in the control flow order if it is not
	 the original loop (i.e. in case of peeling).  */
      if (!first_active_latch)
	{
	  memcpy (first_active, new_bbs, n * sizeof (basic_block));
	  first_active_latch = new_bbs[n - 1];
	}

      /* Set counts and frequencies.  */
      if (flags & DLTHE_FLAG_UPDATE_FREQ)
	{
	  scale_bbs_frequencies (new_bbs, n, scale_act);
	  scale_act = scale_act * scale_step[j];
	}
    }
  free (new_bbs);
  free (orig_loops);

  /* Record the exit edge in the original loop body, and update the frequencies.  */
  if (orig && bitmap_bit_p (wont_exit, 0))
    {
      if (to_remove)
	to_remove->safe_push (orig);
      force_edge_cold (orig, true);

      /* Scale the frequencies of the blocks dominated by the exit.  */
      if (bbs_to_scale && scale_after_exit)
	{
	  EXECUTE_IF_SET_IN_BITMAP (bbs_to_scale, 0, i, bi)
	    scale_bbs_frequencies_profile_count (bbs + i, 1, after_exit_num,
						 after_exit_den);
	}
    }

  /* Update the original loop.  */
  if (!is_latch)
    set_immediate_dominator (CDI_DOMINATORS, e->dest, e->src);
  if (flags & DLTHE_FLAG_UPDATE_FREQ)
    {
      scale_bbs_frequencies (bbs, n, scale_main);
      free (scale_step);
    }

  /* Update dominators of outer blocks if affected.  */
  for (i = 0; i < n; i++)
    {
      basic_block dominated, dom_bb;
      unsigned j;

      bb = bbs[i];

      auto_vec<basic_block> dom_bbs = get_dominated_by (CDI_DOMINATORS, bb);
      FOR_EACH_VEC_ELT (dom_bbs, j, dominated)
	{
	  if (flow_bb_inside_loop_p (loop, dominated))
	    continue;
	  dom_bb = nearest_common_dominator (
			CDI_DOMINATORS, first_active[i], first_active_latch);
	  set_immediate_dominator (CDI_DOMINATORS, dominated, dom_bb);
	}
    }
  free (first_active);

  free (bbs);
  BITMAP_FREE (bbs_to_scale);

  return true;
}

/* A callback for make_forwarder block, to redirect all edges except for
   MFB_KJ_EDGE to the entry part.  E is the edge for that we should decide
   whether to redirect it.  */

edge mfb_kj_edge;
bool
mfb_keep_just (edge e)
{
  return e != mfb_kj_edge;
}

/* True when a candidate preheader BLOCK has predecessors from LOOP.  */

static bool
has_preds_from_loop (basic_block block, class loop *loop)
{
  edge e;
  edge_iterator ei;

  FOR_EACH_EDGE (e, ei, block->preds)
    if (e->src->loop_father == loop)
      return true;
  return false;
}

/* Creates a pre-header for a LOOP.  Returns newly created block.  Unless
   CP_SIMPLE_PREHEADERS is set in FLAGS, we only force LOOP to have single
   entry; otherwise we also force preheader block to have only one successor.
   When CP_FALLTHRU_PREHEADERS is set in FLAGS, we force the preheader block
   to be a fallthru predecessor to the loop header and to have only
   predecessors from outside of the loop.
   The function also updates dominators.  */

basic_block
create_preheader (class loop *loop, int flags)
{
  edge e;
  basic_block dummy;
  int nentry = 0;
  bool irred = false;
  bool latch_edge_was_fallthru;
  edge one_succ_pred = NULL, single_entry = NULL;
  edge_iterator ei;

  FOR_EACH_EDGE (e, ei, loop->header->preds)
    {
      if (e->src == loop->latch)
	continue;
      irred |= (e->flags & EDGE_IRREDUCIBLE_LOOP) != 0;
      nentry++;
      single_entry = e;
      if (single_succ_p (e->src))
	one_succ_pred = e;
    }
  gcc_assert (nentry);
  if (nentry == 1)
    {
      bool need_forwarder_block = false;

      /* We do not allow entry block to be the loop preheader, since we
	     cannot emit code there.  */
      if (single_entry->src == ENTRY_BLOCK_PTR_FOR_FN (cfun))
        need_forwarder_block = true;
      else
        {
          /* If we want simple preheaders, also force the preheader to have
	     just a single successor and a normal edge.  */
          if ((flags & CP_SIMPLE_PREHEADERS)
	      && ((single_entry->flags & EDGE_COMPLEX)
		  || !single_succ_p (single_entry->src)))
            need_forwarder_block = true;
          /* If we want fallthru preheaders, also create forwarder block when
             preheader ends with a jump or has predecessors from loop.  */
          else if ((flags & CP_FALLTHRU_PREHEADERS)
                   && (JUMP_P (BB_END (single_entry->src))
                       || has_preds_from_loop (single_entry->src, loop)))
            need_forwarder_block = true;
        }
      if (! need_forwarder_block)
	return NULL;
    }

  mfb_kj_edge = loop_latch_edge (loop);
  latch_edge_was_fallthru = (mfb_kj_edge->flags & EDGE_FALLTHRU) != 0;
  if (nentry == 1
      && ((flags & CP_FALLTHRU_PREHEADERS) == 0
  	  || (single_entry->flags & EDGE_CROSSING) == 0))
    dummy = split_edge (single_entry);
  else
    {
      edge fallthru = make_forwarder_block (loop->header, mfb_keep_just, NULL);
      dummy = fallthru->src;
      loop->header = fallthru->dest;
    }

  /* Try to be clever in placing the newly created preheader.  The idea is to
     avoid breaking any "fallthruness" relationship between blocks.

     The preheader was created just before the header and all incoming edges
     to the header were redirected to the preheader, except the latch edge.
     So the only problematic case is when this latch edge was a fallthru
     edge: it is not anymore after the preheader creation so we have broken
     the fallthruness.  We're therefore going to look for a better place.  */
  if (latch_edge_was_fallthru)
    {
      if (one_succ_pred)
	e = one_succ_pred;
      else
	e = EDGE_PRED (dummy, 0);

      move_block_after (dummy, e->src);
    }

  if (irred)
    {
      dummy->flags |= BB_IRREDUCIBLE_LOOP;
      single_succ_edge (dummy)->flags |= EDGE_IRREDUCIBLE_LOOP;
    }

  if (dump_file)
    fprintf (dump_file, "Created preheader block for loop %i\n",
	     loop->num);

  if (flags & CP_FALLTHRU_PREHEADERS)
    gcc_assert ((single_succ_edge (dummy)->flags & EDGE_FALLTHRU)
                && !JUMP_P (BB_END (dummy)));

  return dummy;
}

/* Create preheaders for each loop; for meaning of FLAGS see create_preheader.  */

void
create_preheaders (int flags)
{
  if (!current_loops)
    return;

  for (auto loop : loops_list (cfun, 0))
    create_preheader (loop, flags);
  loops_state_set (LOOPS_HAVE_PREHEADERS);
}

/* Forces all loop latches to have only single successor.  */

void
force_single_succ_latches (void)
{
  edge e;

  for (auto loop : loops_list (cfun, 0))
    {
      if (loop->latch != loop->header && single_succ_p (loop->latch))
	continue;

      e = find_edge (loop->latch, loop->header);
      gcc_checking_assert (e != NULL);

      split_edge (e);
    }
  loops_state_set (LOOPS_HAVE_SIMPLE_LATCHES);
}

/* This function is called from loop_version.  It splits the entry edge
   of the loop we want to version, adds the versioning condition, and
   adjust the edges to the two versions of the loop appropriately.
   e is an incoming edge. Returns the basic block containing the
   condition.

   --- edge e ---- > [second_head]

   Split it and insert new conditional expression and adjust edges.

    --- edge e ---> [cond expr] ---> [first_head]
			|
			+---------> [second_head]

  THEN_PROB is the probability of then branch of the condition.
  ELSE_PROB is the probability of else branch. Note that they may be both
  REG_BR_PROB_BASE when condition is IFN_LOOP_VECTORIZED or
  IFN_LOOP_DIST_ALIAS.  */

static basic_block
lv_adjust_loop_entry_edge (basic_block first_head, basic_block second_head,
			   edge e, void *cond_expr,
			   profile_probability then_prob,
			   profile_probability else_prob)
{
  basic_block new_head = NULL;
  edge e1;

  gcc_assert (e->dest == second_head);

  /* Split edge 'e'. This will create a new basic block, where we can
     insert conditional expr.  */
  new_head = split_edge (e);

  lv_add_condition_to_bb (first_head, second_head, new_head,
			  cond_expr);

  /* Don't set EDGE_TRUE_VALUE in RTL mode, as it's invalid there.  */
  e = single_succ_edge (new_head);
  e1 = make_edge (new_head, first_head,
		  current_ir_type () == IR_GIMPLE ? EDGE_TRUE_VALUE : 0);
  e1->probability = then_prob;
  e->probability = else_prob;

  set_immediate_dominator (CDI_DOMINATORS, first_head, new_head);
  set_immediate_dominator (CDI_DOMINATORS, second_head, new_head);

  /* Adjust loop header phi nodes.  */
  lv_adjust_loop_header_phi (first_head, second_head, new_head, e1);

  return new_head;
}

/* Main entry point for Loop Versioning transformation.

   This transformation given a condition and a loop, creates
   -if (condition) { loop_copy1 } else { loop_copy2 },
   where loop_copy1 is the loop transformed in one way, and loop_copy2
   is the loop transformed in another way (or unchanged). COND_EXPR
   may be a run time test for things that were not resolved by static
   analysis (overlapping ranges (anti-aliasing), alignment, etc.).

   If non-NULL, CONDITION_BB is set to the basic block containing the
   condition.

   THEN_PROB is the probability of the then edge of the if.  THEN_SCALE
   is the ratio by that the frequencies in the original loop should
   be scaled.  ELSE_SCALE is the ratio by that the frequencies in the
   new loop should be scaled.

   If PLACE_AFTER is true, we place the new loop after LOOP in the
   instruction stream, otherwise it is placed before LOOP.  */

class loop *
loop_version (class loop *loop,
	      void *cond_expr, basic_block *condition_bb,
	      profile_probability then_prob, profile_probability else_prob,
	      profile_probability then_scale, profile_probability else_scale,
	      bool place_after)
{
  basic_block first_head, second_head;
  edge entry, latch_edge;
  int irred_flag;
  class loop *nloop;
  basic_block cond_bb;

  /* Record entry and latch edges for the loop */
  entry = loop_preheader_edge (loop);
  irred_flag = entry->flags & EDGE_IRREDUCIBLE_LOOP;
  entry->flags &= ~EDGE_IRREDUCIBLE_LOOP;

  /* Note down head of loop as first_head.  */
  first_head = entry->dest;

  /* 1) Duplicate loop on the entry edge.  */
  if (!cfg_hook_duplicate_loop_body_to_header_edge (loop, entry, 1, NULL, NULL,
						    NULL, 0))
    {
      entry->flags |= irred_flag;
      return NULL;
    }

  /* 2) loopify the duplicated new loop. */
  latch_edge = single_succ_edge (get_bb_copy (loop->latch));
  nloop = alloc_loop ();
  class loop *outer = loop_outer (latch_edge->dest->loop_father);
  edge new_header_edge = single_pred_edge (get_bb_copy (loop->header));
  nloop->header = new_header_edge->dest;
  nloop->latch = latch_edge->src;
  loop_redirect_edge (latch_edge, nloop->header);

  /* Compute new loop.  */
  add_loop (nloop, outer);
  copy_loop_info (loop, nloop);
  set_loop_copy (loop, nloop);

  /* loopify redirected latch_edge. Update its PENDING_STMTS.  */
  lv_flush_pending_stmts (latch_edge);

  /* After duplication entry edge now points to new loop head block.
     Note down new head as second_head.  */
  second_head = entry->dest;

  /* 3) Split loop entry edge and insert new block with cond expr.  */
  cond_bb =  lv_adjust_loop_entry_edge (first_head, second_head,
					entry, cond_expr, then_prob, else_prob);
  if (condition_bb)
    *condition_bb = cond_bb;

  if (!cond_bb)
    {
      entry->flags |= irred_flag;
      return NULL;
    }

  /* Add cond_bb to appropriate loop.  */
  if (cond_bb->loop_father)
    remove_bb_from_loops (cond_bb);
  add_bb_to_loop (cond_bb, outer);

  /* 4) Scale the original loop and new loop frequency.  */
  scale_loop_frequencies (loop, then_scale);
  scale_loop_frequencies (nloop, else_scale);
  update_dominators_in_loop (loop);
  update_dominators_in_loop (nloop);

  /* Adjust irreducible flag.  */
  if (irred_flag)
    {
      cond_bb->flags |= BB_IRREDUCIBLE_LOOP;
      loop_preheader_edge (loop)->flags |= EDGE_IRREDUCIBLE_LOOP;
      loop_preheader_edge (nloop)->flags |= EDGE_IRREDUCIBLE_LOOP;
      single_pred_edge (cond_bb)->flags |= EDGE_IRREDUCIBLE_LOOP;
    }

  if (place_after)
    {
      basic_block *bbs = get_loop_body_in_dom_order (nloop), after;
      unsigned i;

      after = loop->latch;

      for (i = 0; i < nloop->num_nodes; i++)
	{
	  move_block_after (bbs[i], after);
	  after = bbs[i];
	}
      free (bbs);
    }

  /* At this point condition_bb is loop preheader with two successors,
     first_head and second_head.   Make sure that loop preheader has only
     one successor.  */
  split_edge (loop_preheader_edge (loop));
  split_edge (loop_preheader_edge (nloop));

  return nloop;
}
