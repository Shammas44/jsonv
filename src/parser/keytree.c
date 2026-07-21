#include "keytree.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void key_tree_init(KeyTreePool *pool, KeyNode *nodes, size_t initial_capacity) {
  /*#region*/
  pool->capacity = initial_capacity;
  pool->nodes = nodes;
  pool->count = 0;
  memset(nodes,0, sizeof(KeyNode)* initial_capacity);
  /*#endregion*/
}

void key_tree_clear(KeyTreePool *self) {
  /*#region*/
  memset(self, 0, sizeof(KeyTreePool));
  /*#endregion*/
}

#include "unescape.h"

// Compares two token strings directly from the AST pool
static int compare_keys(ASTNode *pool, int key1_idx, int key2_idx) {
  /*#region*/
  Token t1 = pool[key1_idx].token;
  Token t2 = pool[key2_idx].token;

  if (!t1.has_escape && !t2.has_escape) {
    int len1 = t1.value.string.length;
    int len2 = t2.value.string.length;
    int min_len = len1 < len2 ? len1 : len2;

    int cmp = strncmp((const char *)t1.value.string.start,
                      (const char *)t2.value.string.start, min_len);
    if (cmp == 0) {
      return len1 - len2;
    }
    return cmp;
  }

  char buf1_stack[256], buf2_stack[256];
  char *u1 = (t1.value.string.length + 1 <= sizeof(buf1_stack)) ? buf1_stack : (char *)malloc(t1.value.string.length + 1);
  char *u2 = (t2.value.string.length + 1 <= sizeof(buf2_stack)) ? buf2_stack : (char *)malloc(t2.value.string.length + 1);

  size_t ulen1 = t1.has_escape ? jsonv_unescape_string(t1.value.string.start, t1.value.string.length, u1) : (memcpy(u1, t1.value.string.start, t1.value.string.length), u1[t1.value.string.length] = '\0', t1.value.string.length);
  size_t ulen2 = t2.has_escape ? jsonv_unescape_string(t2.value.string.start, t2.value.string.length, u2) : (memcpy(u2, t2.value.string.start, t2.value.string.length), u2[t2.value.string.length] = '\0', t2.value.string.length);

  size_t min_len = ulen1 < ulen2 ? ulen1 : ulen2;
  int cmp = strncmp(u1, u2, min_len);
  if (cmp == 0) {
    cmp = (int)ulen1 - (int)ulen2;
  }

  if (u1 != buf1_stack) free(u1);
  if (u2 != buf2_stack) free(u2);

  return cmp;
  /*#endregion*/
}

int key_tree_insert(KeyTreePool *pool, int root_idx, int ast_key_idx,
                    ASTNode *ast_pool) {
  /*#region*/
  if (pool->count >= pool->capacity) {
    return KEY_TREE_IS_FULL;
  }

  // Initialize the new node
  int new_node_idx = pool->count++;
  pool->nodes[new_node_idx].ast_key_idx = ast_key_idx;
  pool->nodes[new_node_idx].left = -1;
  pool->nodes[new_node_idx].right = -1;

  // If the tree is empty, return the new node as the root
  if (root_idx == -1) {
    return new_node_idx;
  }

  // Standard iterative BST insertion (avoids call stack overhead)
  int curr = root_idx;
  while (1) {
    int cmp =
        compare_keys(ast_pool, ast_key_idx, pool->nodes[curr].ast_key_idx);

    if (cmp < 0) {
      if (pool->nodes[curr].left == -1) {
        pool->nodes[curr].left = new_node_idx;
        break;
      } else {
        curr = pool->nodes[curr].left;
      }
    } else {
      // Note: Since duplicate keys are caught by `set_t` prior to this,
      // cmp == 0 should theoretically never happen here. We safely default to
      // right.
      if (pool->nodes[curr].right == -1) {
        pool->nodes[curr].right = new_node_idx;
        break;
      } else {
        curr = pool->nodes[curr].right;
      }
    }
  }

  return root_idx;
  /*#endregion*/
}

// Helper to populate the output array recursively
static void get_ordered_recursive(KeyTreePool *pool, int node_idx,
                                  int *output_array, int *output_count) {
  /*#region*/
  if (node_idx == -1)
    return;

  // In-order traversal: Left, Node, Right
  get_ordered_recursive(pool, pool->nodes[node_idx].left, output_array,
                        output_count);

  output_array[(*output_count)++] = pool->nodes[node_idx].ast_key_idx;

  get_ordered_recursive(pool, pool->nodes[node_idx].right, output_array,
                        output_count);
  /*#endregion*/
}

void key_tree_get_ordered(KeyTreePool *pool, int root_idx, int *output_array,
                          int *output_count) {
  /*#region*/
  *output_count = 0;
  get_ordered_recursive(pool, root_idx, output_array, output_count);
  /*#endregion*/
}
