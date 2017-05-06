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


static inline int shack_hash(target_ulong guest_eip){
    return (int)guest_eip % SHACK_HTABLE_SIZE;
}

static inline void shack_init(CPUState *env)
{

    env->shack = (uint64_t*) malloc(sizeof(uint64_t) * SHACK_SIZE);
    env->shadow_hash_list = (void*) malloc(sizeof(list_t) * SHACK_HTABLE_SIZE);
    env->shadow_ret_addr = (unsigned long**) malloc(sizeof(unsigned long*) * SHACK_SIZE);

    shadow_hash_list = env->shadow_hash_list;

    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    memset(env->shadow_hash_list, 0, sizeof(list_t) * SHACK_HTABLE_SIZE);
    memset(env->shadow_ret_addr, 0, sizeof(unsigned long*) * SHACK_SIZE);

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
    struct shadow_pair *pair;
    int h = shack_hash(guest_eip);
    list_t *head = &shadow_hash_list[h];
    head = head->next;
    while(head){
        pair = (struct shadow_pair *)head;
        if(pair->guest_eip == guest_eip){
            *(pair->shadow_slot) = host_eip;
            //printf("shack_set:gip = 0x%x, hip = %p\n", guest_eip, host_eip);
            break;
        }
        head = head->next;
    }
    
    /* free a pair */
    /* 
    if(target){
        target->prev->next = target->next;
        if(target->next){
            target->next->prev = target->prev;
        }
        target->next = NULL;
        target->prev = NULL;
        free((struct shadow_pair*)target);
        //env->shadow_ret_count--;
    }
    */
}

/*
 * helper_shack_flush()
 *  Reset shadow stack.
 */
void helper_shack_flush(CPUState *env)
{
    list_t *l, *nl;
    int i;
    for(i = 0; i < SHACK_HTABLE_SIZE; i++){
        l = shadow_hash_list[i].next;
        while(l){
            nl = l->next;
            free(l);
            l = nl;
        }
    }
    memset(env->shack, 0, sizeof(uint64_t) * SHACK_SIZE);
    memset(env->shadow_hash_list, 0, sizeof(list_t) * SHACK_HTABLE_SIZE);
    memset(env->shadow_ret_addr, 0, sizeof(unsigned long) * SHACK_SIZE);
    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE ;
    env->shadow_ret_count = 0;
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */

void helper_hello (unsigned int v, target_ulong gip, target_ulong hip)
{
    printf("push = %d, gip = 0x%x, hip = 0x%x\n", (v>>3), gip, hip);
}
void helper_hello2 (unsigned int v, target_ulong gip, target_ulong hip)
{
    printf("pop  = %d, gip = 0x%x, hip = 0x%x\n",(v >>3), gip, hip);
}
void helper_ptl(target_ulong tll)
{
    printf("tl = 0x%x\n", tll);
}
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
    /* check if next_eip has corresponding tb*/
    TranslationBlock *tb, **ptb1;
    target_ulong pc, cs_base;
    unsigned long * tc_ptr;
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
    tc_ptr = NULL;
found_t:
    /* we add the TB in the virtual pc hash table */
    //env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)] = tb;
    // host
    if(found){
        tc_ptr = (unsigned long *)tb->tc_ptr;
        //printf("found!\n");
    }

    /* tcg gen code start */

    int L_NFULL = gen_new_label();
    TCGv_ptr temp_shack_top = tcg_temp_local_new_ptr();
    TCGv_ptr temp_shack_end = tcg_temp_local_new_ptr();
    TCGv_ptr temp_shadow_ret_addr = tcg_temp_local_new_ptr();
    TCGv_ptr temp_host_ret_addr = tcg_const_local_tl((uint32_t)tc_ptr);
    TCGv_ptr temp_guest_ret_addr = tcg_const_local_tl(next_eip);
    /* if shack full then flush */
    tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(temp_shack_end, cpu_env, offsetof(CPUState, shack_end));
    //gen_helper_ptl(temp_shack_top); 
    tcg_gen_brcond_tl(TCG_COND_NE, temp_shack_top, temp_shack_end, L_NFULL);
    /* stack full, flush shack*/
    gen_helper_shack_flush(cpu_env);
    /* stack not full */
    gen_set_label(L_NFULL);
    // push guest_ret_addr to shack
    tcg_gen_st_tl(temp_guest_ret_addr, temp_shack_top,0);
    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, 8);
    tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    //gen_helper_ptl(temp_shack_top);
    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, -8);

    if(!found){
        //not found!!
        //lookup hashtable
        h = shack_hash(next_eip);
        list_t *head = &shadow_hash_list[h];
        //creat shadow_pair
        struct shadow_pair *new_node = malloc(sizeof(struct shadow_pair));
        list_t *new =  &(new_node->l);
        
        new_node->guest_eip = next_eip;
        new_node->shadow_slot = &env->shadow_ret_addr[env->shadow_ret_count];
        //insert shadow_pair into hashtable
        new->next = head->next;
        new->prev = head;
        if(head->next)
            head->next->prev = new;
        head->next = new;

        // generrate tcg code to load host_ret_addr from corresponding shadow_ret_addr
        tcg_gen_ld_ptr(temp_shadow_ret_addr, cpu_env, offsetof(CPUState, shadow_ret_addr));
        tcg_gen_addi_ptr(temp_shadow_ret_addr, temp_shadow_ret_addr, env->shadow_ret_count * sizeof(unsigned long *));
        tcg_gen_ld_tl(temp_host_ret_addr, temp_shadow_ret_addr, 0); 
        env->shadow_ret_count++;
    }
    tcg_gen_st_tl(temp_host_ret_addr, temp_shack_top, 4);
    //tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, 8);
    //gen_helper_ptl(temp_shack_top); 
    //tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    //tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    //gen_helper_ptl(temp_shack_top); 

#if 0
    TCGv_ptr temp_shack = tcg_temp_new_ptr();
    tcg_gen_ld_ptr(temp_shack, cpu_env, offsetof(CPUState, shack));
    tcg_gen_sub_tl(temp_shack, temp_shack_top, temp_shack); 
    gen_helper_hello(temp_shack, temp_guest_ret_addr, temp_host_ret_addr);
    tcg_temp_free(temp_shack);
#endif
    tcg_temp_free(temp_shack_top);
    tcg_temp_free(temp_shack_end);
    tcg_temp_free(temp_shadow_ret_addr);
    tcg_temp_free(temp_host_ret_addr);
    tcg_temp_free(temp_guest_ret_addr);
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
    TCGv m_next_eip = tcg_temp_local_new();

    int L_EMPTY = gen_new_label();
    int L_NEQ = gen_new_label();
    tcg_gen_mov_tl(m_next_eip, next_eip);
    tcg_gen_ld_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(temp_shack, cpu_env, offsetof(CPUState, shack));

    tcg_gen_brcond_tl(TCG_COND_EQ, temp_shack_top, temp_shack, L_EMPTY);

    tcg_gen_addi_ptr(temp_shack_top, temp_shack_top, -8);
    tcg_gen_ld_tl(s_next_eip, temp_shack_top, 0);
    tcg_gen_ld_tl(s_host_eip, temp_shack_top, 4);
    /* how to compare and set jump?  */
    tcg_gen_st_ptr(temp_shack_top, cpu_env, offsetof(CPUState, shack_top));

    tcg_gen_brcond_tl(TCG_COND_NE, s_next_eip, m_next_eip, L_NEQ);
    tcg_gen_brcond_tl(TCG_COND_EQ, s_host_eip, tcg_const_tl(0), L_NEQ);
    
    *gen_opc_ptr++ = INDEX_op_jmp;
    *gen_opparam_ptr++ = GET_TCGV_i32(s_host_eip);
    gen_set_label(L_NEQ);
    gen_set_label(L_EMPTY);

#if  0
    tcg_gen_sub_tl(temp_shack, temp_shack_top, temp_shack); 
    gen_helper_hello2(temp_shack, s_next_eip, s_host_eip);
#endif

    tcg_temp_free(temp_shack_top);
    tcg_temp_free(temp_shack);
    tcg_temp_free(s_next_eip);
    tcg_temp_free(s_host_eip);
    tcg_temp_free(m_next_eip);

}

/*
 * Indirect Branch Target Cache
 */
__thread int update_ibtc;

static struct ibtc_table ibtc_table;

/*
 * helper_lookup_ibtc()
 *  Look up IBTC. Return next host eip if cache hit or
 *  back-to-dispatcher stub address if cache miss.
 */
void *helper_lookup_ibtc(target_ulong guest_eip)
{
    int index = guest_eip & IBTC_CACHE_MASK;
    struct jmp_pair *jp = &ibtc_table.htable[index];
    if(jp && jp->guest_eip == guest_eip )
    {
        return jp->tb->tc_ptr;
    }
    update_ibtc = 1;
    return optimization_ret_addr;
}

/*
 * update_ibtc_entry()
 *  Populate eip and tb pair in IBTC entry.
 */
void update_ibtc_entry(TranslationBlock *tb)
{
    target_ulong guest_eip = tb->pc;
    int index = guest_eip & IBTC_CACHE_MASK;
    ibtc_table.htable[index].guest_eip = guest_eip;
    ibtc_table.htable[index].tb = tb;
    update_ibtc = 0;
}

/*
 * ibtc_init()
 *  Create and initialize indirect branch target cache.
 */
static inline void ibtc_init(CPUState *env)
{
    //env->ibtc_table = (struct ibtc_table*)malloc(sizeof(struct ibtc_table));
    memset(&ibtc_table, 0, sizeof(struct ibtc_table));
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
