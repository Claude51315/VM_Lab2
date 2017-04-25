/*
 *  (C) 2010 by Computer System Laboratory, IIS, Academia Sinica, Taiwan.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "exec-all.h"
#include "tcg-op.h"
#include "helper.h"
#define GEN_HELPER 1
#include "helper.h"
#include "optimization.h"

extern uint8_t *optimization_ret_addr;

/*
 * Shadow Stack
 */
list_t *shadow_hash_list;

static inline void shack_init(CPUState *env)
{

    env->shack = (uint64_t*) malloc(sizeof(uint64_t) * SHACK_SIZE);
    env->shadow_hash_list = (void*) malloc(sizeof(struct shadow_pair) * SHACK_SIZE);
    env->shadow_ret_addr = (unsigned long*) malloc(sizeof(unsigned long) * SHACK_SIZE);
    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    memset(env->shadow_hash_list, 0, sizeof(struct shadow_pair) * SHACK_SIZE);
    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE ;
    
}

/*
 * shack_set_shadow()
 *  Insert a guest eip to host eip pair if it is not yet created.
 */
 void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
}

/*
 * helper_shack_flush()
 *  Reset shadow stack.
 */
void helper_shack_flush(CPUState *env)
{
    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    memset(env->shadow_hash_list, 0, sizeof(struct shadow_pair) * SHACK_SIZE);
    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE ;
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void helper_hello ()
{
    printf("hello QEMU!\n");
}
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
    gen_helper_hello();
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
}

/*
 * Indirect Branch Target Cache
 */
__thread int update_ibtc;

/*
 * helper_lookup_ibtc()
 *  Look up IBTC. Return next host eip if cache hit or
 *  back-to-dispatcher stub address if cache miss.
 */
void *helper_lookup_ibtc(target_ulong guest_eip)
{
    return optimization_ret_addr;
}

/*
 * update_ibtc_entry()
 *  Populate eip and tb pair in IBTC entry.
 */
void update_ibtc_entry(TranslationBlock *tb)
{
}

/*
 * ibtc_init()
 *  Create and initialize indirect branch target cache.
 */
static inline void ibtc_init(CPUState *env)
{
}

/*
 * init_optimizations()
 *  Initialize optimization subsystem.
 */
int init_optimizations(CPUState *env)
{
    shack_init(env);
    ibtc_init(env);

    return 0;
}

/*
 * vim: ts=8 sts=4 sw=4 expandtab
 */
