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
    env->shadow_hash_list = (void*) malloc(sizeof(list_t) * SHACK_HTABLE_SIZE);
    env->shadow_ret_addr = (unsigned long*) malloc(sizeof(unsigned long) * SHACK_SIZE);

    shadow_hash_list = env->shadow_hash_list;

    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    memset(env->shadow_hash_list, 0, sizeof(list_t) * SHACK_HTABLE_SIZE);
    memset(env->shadow_ret_addr, 0, sizeof(unsigned long) * SHACK_SIZE);

    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE;
    env->shadow_ret_count = 0;
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
    memset(env->shadow_ret_addr, 0, sizeof(unsigned long) * SHACK_SIZE);
    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE ;
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */

void helper_hello (unsigned int v)
{
    printf("push = %d\n",(v>>3));
}
void helper_hello2 (unsigned int v)
{
    printf("pop = %d\n",(v >>3));
}
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{

    /* check if next_eip has corresponding tb*/
    TranslationBlock *tb, **ptb1;
    target_ulong pc, cs_base, tc_ptr;
    int flags, found;
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
    found = 1;
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
    found = 0;
    tc_ptr = 0;
found_t:
    /* we add the TB in the virtual pc hash table */
    //env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)] = tb;
    // host
    if(found){
        tc_ptr = (target_ulong) tb->tc_ptr;
    }

    /* tcg gen code start */

    int L_NFULL = gen_new_label();
    TCGv_ptr temp_shack_top = tcg_temp_local_new_ptr();
    TCGv_ptr temp_shack_end = tcg_temp_local_new_ptr();
    TCGv_ptr temp_shadow_ret_addr = tcg_temp_local_new_ptr();

    tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(temp_shack_end, cpu_env, offsetof(CPUState, shack_end));
    tcg_gen_brcond_tl(TCG_COND_NE, temp_shack_top, temp_shack_end, L_NFULL);
    /* stack full, flush shack*/
    gen_helper_shack_flush(cpu_env);
    gen_set_label(L_NFULL);

    tcg_gen_st_tl(tcg_const_tl(next_eip), temp_shack_top,0);

    if(found)
        tcg_gen_st_tl(tcg_const_tl(tc_ptr), temp_shack_top,4);
    else {
        h = next_eip % SHACK_HTABLE_SIZE;
        list_t *head = &shadow_hash_list[h];
        struct shadow_pair *new_node = malloc(sizeof(struct shadow_pair));
        list_t *new =  &(new_node->l);

        new->next = head->next;
        new->prev = head;
        if(head->next)
            head->next->prev = new;
        head->next = new;

        new_node->guest_eip = next_eip;
        new_node->shadow_slot = &env->shadow_ret_addr[env->shadow_ret_count];
        // generrate tcg code
        tcg_gen_ld_ptr(temp_shadow_ret_addr, cpu_env, offsetof(CPUState, shadow_ret_addr));
        tcg_gen_ld_tl(temp_shadow_ret_addr, temp_shadow_ret_addr, env->shadow_ret_count * sizeof(unsigned long)); 
        tcg_gen_st_tl(temp_shadow_ret_addr, temp_shack_top, 4);
        env->shadow_ret_count++;
    }


    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, 8);
    tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));

    /* debug 
       TCGv_ptr temp_shack = tcg_temp_new_ptr();
       tcg_gen_ld_ptr(temp_shack, cpu_env, offsetof(CPUState, shack));
       tcg_gen_sub_tl(temp_shack, temp_shack_top, temp_shack); 
       gen_helper_hello(temp_shack);
     */
    tcg_temp_free(temp_shack_top);
    tcg_temp_free(temp_shack_end);
    tcg_temp_free(temp_shadow_ret_addr);
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{

    TCGv_ptr temp_shack_top = tcg_temp_local_new_ptr();
    TCGv_ptr temp_shack = tcg_temp_local_new_ptr();
    TCGv s_next_eip = tcg_temp_local_new();
    TCGv s_host_eip = tcg_temp_local_new();
    int L_EMPTY = gen_new_label();

    tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(temp_shack, cpu_env, offsetof(CPUState, shack));

    tcg_gen_brcond_tl(TCG_COND_EQ, temp_shack_top, temp_shack, L_EMPTY);

    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, -8);
    tcg_gen_ld_tl(s_next_eip, temp_shack_top, 0);
    tcg_gen_ld_tl(s_host_eip, temp_shack_top, 4);
    /* how to compare and set jump?  */

    tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));


    gen_set_label(L_EMPTY);
    /* debug 
       tcg_gen_sub_tl(temp_shack, temp_shack_top, temp_shack); 
       gen_helper_hello2(temp_shack);
     */


    tcg_temp_free(temp_shack_top);
    tcg_temp_free(temp_shack);
    tcg_temp_free(s_next_eip);
    tcg_temp_free(s_host_eip);

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
