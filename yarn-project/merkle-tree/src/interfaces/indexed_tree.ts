import { IndexedTreeLeaf, IndexedTreeLeafPreimage, SiblingPath } from '@aztec/types';

import { AppendOnlyTree } from './append_only_tree.js';

/* eslint-disable */

/**
 * All of the data to be return during batch insertion.
 */
export interface LowLeafWitnessData<N extends number, Leaf extends IndexedTreeLeaf> {
  /**
   * Preimage of the low nullifier that proves non membership.
   */
  leafData: IndexedTreeLeafPreimage<Leaf>;
  /**
   * Sibling path to prove membership of low nullifier.
   */
  siblingPath: SiblingPath<N>;
  /**
   * The index of low nullifier.
   */
  index: bigint;
}

/**
 * The result of a batch insertion in an indexed merkle tree.
 */
export interface BatchInsertionResult<
  TreeHeight extends number,
  SubtreeSiblingPathHeight extends number,
  Leaf extends IndexedTreeLeaf,
> {
  /**
   * Data for the leaves to be updated when inserting the new ones.
   */
  lowLeavesWitnessData?: LowLeafWitnessData<TreeHeight, Leaf>[];
  /**
   * Sibling path "pointing to" where the new subtree should be inserted into the tree.
   */
  newSubtreeSiblingPath: SiblingPath<SubtreeSiblingPathHeight>;
  /**
   * The new leaves being inserted in high to low order. This order corresponds with the order of the low leaves witness.
   */
  sortedNewLeaves: Buffer[];
  /**
   * The indexes of the sorted new leaves to the original ones.
   */
  sortedNewLeavesIndexes: number[];
}

/**
 * Indexed merkle tree.
 */
export interface IndexedTree<Leaf extends IndexedTreeLeaf> extends AppendOnlyTree {
  /**
   * Finds the index of the largest leaf whose value is less than or equal to the provided value.
   * @param newValue - The new value to be inserted into the tree.
   * @param includeUncommitted - If true, the uncommitted changes are included in the search.
   * @returns The found leaf index and a flag indicating if the corresponding leaf's value is equal to `newValue`.
   */
  findIndexOfPreviousKey(
    newValue: bigint,
    includeUncommitted: boolean,
  ): {
    /**
     * The index of the found leaf.
     */
    index: number;
    /**
     * A flag indicating if the corresponding leaf's value is equal to `newValue`.
     */
    alreadyPresent: boolean;
  };

  /**
   * Gets the latest LeafPreimage copy.
   * @param index - Index of the leaf of which to obtain the LeafPreimage copy.
   * @param includeUncommitted - If true, the uncommitted changes are included in the search.
   * @returns A copy of the leaf preimage at the given index or undefined if the leaf was not found.
   */
  getLatestLeafPreimageCopy(index: number, includeUncommitted: boolean): IndexedTreeLeafPreimage<Leaf> | undefined;

  /**
   * Batch insert multiple leaves into the tree.
   * @param leaves - Leaves to insert into the tree.
   * @param subtreeHeight - Height of the subtree.
   * @param includeUncommitted - If true, the uncommitted changes are included in the search.
   */
  batchInsert<TreeHeight extends number, SubtreeHeight extends number, SubtreeSiblingPathHeight extends number>(
    leaves: Buffer[],
    subtreeHeight: SubtreeHeight,
    includeUncommitted: boolean,
  ): Promise<BatchInsertionResult<TreeHeight, SubtreeSiblingPathHeight, Leaf>>;
}
