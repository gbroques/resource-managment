#ifndef RESOURCE_H
#define RESOURCE_H

#define MAX_INSTANCES 10
#define MAX_REQUESTS  20

struct res_node {
  unsigned int num_instances;
  unsigned int num_allocated;
  unsigned int type;
  int shareable;
  struct proc_node* held_by[MAX_INSTANCES];
};

struct proc_node {
  unsigned int id;
  struct res_node* requests[MAX_REQUESTS];
};

#endif