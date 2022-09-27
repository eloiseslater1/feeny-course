    .globl call_feeny
    .globl trap
call_feeny :
    movq %rdi, %rax
    movq top_of_heap(%rip), %rdi
    movq heap_pointer(%rip) ,%rsi
    movq stack_pointer(%rip), %rdx
    movq frame_pointer(%rip), %rcx
    call *%rax
    movq %rsi, heap_pointer(%rip)
    movq %rdx, heap_pointer(%rip)
    movq %rcx, frame_pointer(%rip)
    ret
# 
trap :
    leaq after_trap(%rip), %rax
    movq $0xcafebabecafebabe, %r8 
    movq %rax, (%r8)
    movq $0xcafebabecafebabe, %rax
    ret
after_trap :


