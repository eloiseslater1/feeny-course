    .globl return_ins
    .globl return_ins_end
    .globl inc_counter_ins
    .globl inc_counter_ins_end
return_ins :
    movq $0xcafebabecafebabe , %rax
    ret
return_ins_end :

inc_counter_ins :
    movq $0xcafebabecafebabe, %rcx
    movq (%rcx), %rax
    addq $1, %rax
    movq %rax , (%rcx)
    ret
inc_counter_ins_end :

