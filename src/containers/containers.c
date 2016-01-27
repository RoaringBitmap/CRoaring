
#include "containers.h"

extern char * get_container_name(uint8_t typecode);

extern uint32_t container_get_cardinality(void *container, uint8_t typecode);

extern bool container_nonzero_cardinality(void *container, uint8_t typecode);

extern void container_free( void *container, uint8_t typecode);

extern int container_to_uint32_array( uint32_t *output, void *container, uint8_t typecode, uint32_t base);

extern void *container_add(  void *container, uint16_t val, uint8_t typecode, uint8_t *new_typecode);

extern bool container_contains(  void *container, uint16_t val, uint8_t typecode);

extern void *container_clone(void *container, uint8_t typecode);

extern void *container_and(void *c1, uint8_t type1, void *c2, uint8_t type2, uint8_t *result_type);

extern void *container_or(void *c1, uint8_t type1, void *c2, uint8_t type2,
		uint8_t *result_type) ;
