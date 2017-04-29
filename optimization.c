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
 
void helper_hello (unsigned int v)
{
    printf("push = %u\n",16384 - (v>>3));
}
void helper_hello2 (unsigned int v)
{
    printf("pop = %u\n",16384 - (v >>3));
}
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
    
    TranslationBlock *tb, **ptb1;
    target_ulong pc, cs_base, tc_ptr;
    int flags;
    unsigned int h;
    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
    pc = cs_base + next_eip;
    tb_page_addr_t phys_pc, phys_page1, phys_page2 ;
    target_ulong virt_page2;
   
    phys_pc = get_page_addr_code(env, pc);
    
    phys_page1 = phys_pc & TARGET_PAGE_MASK;
    phys_page2 = -1;
    h = tb_phys_hash_func(phys_pc);
    ptb1 = &tb_phys_hash[h];
    for(;;) {
        tb = *ptb1;
        if (!tb)
            goto not_found_t;
        if (tb->pc == pc &&
            tb->page_addr[0] == phys_page1 &&
            tb->cs_base == cs_base &&
            tb->flags == flags) {
            /* check next page if needed */
            if (tb->page_addr[1] != -1) {
                virt_page2 = (pc & TARGET_PAGE_MASK) +
                    TARGET_PAGE_SIZE;
                phys_page2 = get_page_addr_code(env, virt_page2);
                if (tb->page_addr[1] == phys_page2)
                    goto found_t;
            } else {
                goto found_t;
            }
        }
        ptb1 = &tb->phys_hash_next;
    }
 not_found_t:
   /* if no translated code available, then translate it now */
   // tb = tb_gen_code(env, pc, cs_base, flags, 0);
    
 found_t:
    /* we add the TB in the virtual pc hash table */
    //env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)] = tb;
    // host
    tc_ptr = tb->tc_ptr;

///////////////////////////////////////////////////////
    TCGv_ptr temp_shack_top = tcg_temp_new_ptr();
    TCGv_ptr temp_shack_end = tcg_temp_new_ptr();
    size_t r= env->shack_end - env->shack_top;
    
   //if(r == 0){
   //    gen_helper_shack_flush(env);
   
   // }
    static uint32_t qqq = 1; 

    // temp_shack = (env->shack_top)
    tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(temp_shack_end, cpu_env, offsetof(CPUState, shack_end));
    
    // *(env->shack_top) = qqq;
    tcg_gen_st_tl(tcg_const_tl(qqq++), temp_shack_top,0);
    tcg_gen_st_tl(tcg_const_tl(qqq++), temp_shack_top,4);
    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, 8);
    tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    
    
    tcg_gen_sub_tl(temp_shack_end, temp_shack_end, temp_shack_top); 
    gen_helper_hello(temp_shack_end);
    
    tcg_temp_free(temp_shack_top);
    tcg_temp_free(temp_shack_end);
    
    
    //printf("remain = %lu\n",env->shack_end - env->shack_top);
    // env->shack_top ++ ;
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
    TCGv_ptr temp_shack_top = tcg_temp_new_ptr();
    TCGv_ptr temp_shack_end = tcg_temp_new_ptr();
    TCGv qq1,qq2;
    qq1 = tcg_temp_new();
    qq2 = tcg_temp_new();
    // temp_shack = (env->shack_top)
    tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(temp_shack_end, cpu_env, offsetof(CPUState, shack_end));
    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, -8);
    tcg_gen_sub_tl(temp_shack_end, temp_shack_end, temp_shack_top); 
    gen_helper_hello2(temp_shack_end);

    tcg_gen_ld_tl(qq1, temp_shack_top,0);
    tcg_gen_ld_tl(qq2, temp_shack_top,4);
    //tcg_gen_ld_tl(qq1, temp_shack_top,0);
    //gen_helper_hello(qq1);
    tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_temp_free(temp_shack_top);
    tcg_temp_free(temp_shack_end);
    tcg_temp_free(qq1);
    tcg_temp_free(qq2);
   ////// 

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
