#ifndef RESOURCE_H
#define RESOURCE_H

#define MAX_INSTANCES 10

struct res_node {
  unsigned int type;
  unsigned int num_instances;
  unsigned int num_allocated;
  int shareable;
  int held_by[MAX_INSTANCES];
};

struct proc_node {
  unsigned int id;
  int request;
};

int get_res_instance(struct res_node* res);

#endif