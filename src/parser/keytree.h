#ifndef _JSONV_KEY_TREE_H_INCLUDED
#define _JSONV_KEY_TREE_H_INCLUDED
#include "ast.h"

// Represents a node in the Binary Search Tree
typedef struct {
    int ast_key_idx; // Index of the key node in the main AST pool
    int left;        // Left child index in the KeyTreePool
    int right;       // Right child index in the KeyTreePool
} KeyNode;

#define KEY_TREE_IS_FULL -1

// A contiguous memory pool for high-performance tree allocation
typedef struct {
    KeyNode *nodes;
    size_t capacity;
    size_t count;
} KeyTreePool;

// Initializes the pool
void key_tree_init(KeyTreePool *pool, KeyNode* nodes, size_t initial_capacity);

// Frees the pool memory
void key_tree_free(KeyTreePool *pool);

// Inserts a new key into the binary search tree of a specific object.
// Returns the new root index (useful when the tree is empty and root is -1).
int key_tree_insert(KeyTreePool *pool, int root_idx, int ast_key_idx, ASTNode *ast_pool);

// Recursively extracts AST node indices in alphabetical order
void key_tree_get_ordered(KeyTreePool *pool, int root_idx, int *output_array, int *output_count);

#endif 
