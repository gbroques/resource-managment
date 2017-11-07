#include "resource.h"

/**
 * Gets a resource instance
 *
 * @param res The requested resource
 *
 * @return -1 if no instances are available
 */
int get_res_instance(struct res_node* res) {
  // Find next available index
  int k = 0;
  while (res->held_by[k] != -1 && k < MAX_INSTANCES) {
    k++;
  }

  if (k == MAX_INSTANCES) {
    return -1; // No more instances available
  } else {
    return k;
  }
}