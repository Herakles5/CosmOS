0000000000004040 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj>:
    4040:	48 85 ff             	test   %rdi,%rdi
    4043:	0f 84 dc 01 00 00    	je     4225 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1e5>
    4049:	41 57                	push   %r15
    404b:	41 56                	push   %r14
    404d:	41 55                	push   %r13
    404f:	41 54                	push   %r12
    4051:	55                   	push   %rbp
    4052:	53                   	push   %rbx
    4053:	89 f3                	mov    %esi,%ebx
    4055:	48 83 ec 38          	sub    $0x38,%rsp
    4059:	81 fe ff 00 00 00    	cmp    $0xff,%esi
    405f:	0f 87 b9 01 00 00    	ja     421e <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1de>
    4065:	89 f0                	mov    %esi,%eax
    4067:	49 89 fc             	mov    %rdi,%r12
    406a:	48 8d 14 40          	lea    (%rax,%rax,2),%rdx
    406e:	4c 8d 2c 90          	lea    (%rax,%rdx,4),%r13
    4072:	49 c1 e5 04          	shl    $0x4,%r13
    4076:	4a 8d 2c 2f          	lea    (%rdi,%r13,1),%rbp
    407a:	80 bd 35 01 00 00 00 	cmpb   $0x0,0x135(%rbp)
    4081:	0f 84 89 01 00 00    	je     4210 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1d0>
    4087:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			408a: R_X86_64_32S	.rodata.str1.8+0x1240
    408e:	31 c0                	xor    %eax,%eax
    4090:	e8 00 00 00 00       	call   4095 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x55>
			4091: R_X86_64_PLT32	printf-0x4
    4095:	89 de                	mov    %ebx,%esi
    4097:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			409a: R_X86_64_32S	.rodata.str1.8+0x12c0
    409e:	31 c0                	xor    %eax,%eax
    40a0:	e8 00 00 00 00       	call   40a5 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x65>
			40a1: R_X86_64_PLT32	printf-0x4
    40a5:	89 de                	mov    %ebx,%esi
    40a7:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			40aa: R_X86_64_32S	.rodata.str1.8+0x12e8
    40ae:	31 c0                	xor    %eax,%eax
    40b0:	e8 00 00 00 00       	call   40b5 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x75>
			40b1: R_X86_64_PLT32	printf-0x4
    40b5:	89 de                	mov    %ebx,%esi
    40b7:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			40ba: R_X86_64_32S	.rodata.str1.8+0x1240
    40be:	31 c0                	xor    %eax,%eax
    40c0:	e8 00 00 00 00       	call   40c5 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x85>
			40c1: R_X86_64_PLT32	printf-0x4
    40c5:	9c                   	pushf
    40c6:	5e                   	pop    %rsi
    40c7:	49 89 e6             	mov    %rsp,%r14
    40ca:	bf 20 68 00 00       	mov    $0x6820,%edi
    40cf:	48 83 ce 02          	or     $0x2,%rsi
    40d3:	e8 00 00 00 00       	call   40d8 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x98>
			40d4: R_X86_64_PLT32	cosmos_vmwrite-0x4
    40d8:	4c 89 f6             	mov    %r14,%rsi
    40db:	bf 1c 68 00 00       	mov    $0x681c,%edi
    40e0:	e8 00 00 00 00       	call   40e5 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0xa5>
			40e1: R_X86_64_PLT32	cosmos_vmwrite-0x4
    40e5:	0f 20 de             	mov    %cr3,%rsi
    40e8:	bf 02 68 00 00       	mov    $0x6802,%edi
    40ed:	e8 00 00 00 00       	call   40f2 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0xb2>
			40ee: R_X86_64_PLT32	cosmos_vmwrite-0x4
    40f2:	48 c7 c6 00 00 00 00 	mov    $0x0,%rsi
			40f5: R_X86_64_32S	.text+0x41d0
    40f9:	bf 1e 68 00 00       	mov    $0x681e,%edi
    40fe:	e8 00 00 00 00       	call   4103 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0xc3>
			40ff: R_X86_64_PLT32	cosmos_vmwrite-0x4
    4103:	49 89 ea             	mov    %rbp,%r10
    4106:	4c 89 54 24 18       	mov    %r10,0x18(%rsp)
    410b:	49 89 d9             	mov    %rbx,%r9
    410e:	4c 89 4c 24 10       	mov    %r9,0x10(%rsp)
    4113:	4d 89 e0             	mov    %r12,%r8
    4116:	4c 89 44 24 08       	mov    %r8,0x8(%rsp)
    411b:	4c 89 e9             	mov    %r13,%rcx
    411e:	48 89 0c 24          	mov    %rcx,(%rsp)
    4122:	4d 89 f7             	mov    %r14,%r15
    4125:	4d 89 fe             	mov    %r15,%r14
    4128:	4f 8d a4 2c 98 00 00 	lea    0x98(%r12,%r13,1),%r12
    412f:	00 
    4130:	ba 78 00 00 00       	mov    $0x78,%edx
    4135:	31 f6                	xor    %esi,%esi
    4137:	4c 89 e7             	mov    %r12,%rdi
    413a:	e8 00 00 00 00       	call   413f <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0xff>
			413b: R_X86_64_PLT32	memset-0x4
    413f:	4c 8b 54 24 18       	mov    0x18(%rsp),%r10
    4144:	48 8b 0c 24          	mov    (%rsp),%rcx
    4148:	89 de                	mov    %ebx,%esi
    414a:	4c 8b 4c 24 10       	mov    0x10(%rsp),%r9
    414f:	4c 8b 44 24 08       	mov    0x8(%rsp),%r8
    4154:	31 c0                	xor    %eax,%eax
    4156:	4c 89 bd 00 01 00 00 	mov    %r15,0x100(%rbp)
    415d:	4c 89 95 c8 00 00 00 	mov    %r10,0xc8(%rbp)
    4164:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			4167: R_X86_64_32S	.rodata.str1.8+0x1318
    416b:	4c 89 8d a0 00 00 00 	mov    %r9,0xa0(%rbp)
    4172:	4c 89 85 f0 00 00 00 	mov    %r8,0xf0(%rbp)
    4179:	48 89 8d f8 00 00 00 	mov    %rcx,0xf8(%rbp)
    4180:	4c 89 b5 08 01 00 00 	mov    %r14,0x108(%rbp)
    4187:	e8 00 00 00 00       	call   418c <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x14c>
			4188: R_X86_64_PLT32	printf-0x4
    418c:	48 c7 c0 00 00 00 00 	mov    $0x0,%rax
			418f: R_X86_64_32S	.text+0x41d0
    4193:	4c 89 e7             	mov    %r12,%rdi
    4196:	e8 00 00 00 00       	call   419b <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x15b>
			4197: R_X86_64_PLT32	cosmos_vmlaunch_trampoline-0x4
    419b:	48 8d 74 24 28       	lea    0x28(%rsp),%rsi
    41a0:	bf 00 44 00 00       	mov    $0x4400,%edi
    41a5:	48 c7 44 24 28 00 00 	movq   $0x0,0x28(%rsp)
    41ac:	00 00 
    41ae:	e8 00 00 00 00       	call   41b3 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x173>
			41af: R_X86_64_PLT32	cosmos_vmread-0x4
    41b3:	48 8b 54 24 28       	mov    0x28(%rsp),%rdx
    41b8:	89 de                	mov    %ebx,%esi
    41ba:	31 c0                	xor    %eax,%eax
    41bc:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			41bf: R_X86_64_32S	.rodata.str1.8+0x1340
    41c3:	e8 00 00 00 00       	call   41c8 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x188>
			41c4: R_X86_64_PLT32	printf-0x4
    41c8:	b8 fe ff ff ff       	mov    $0xfffffffe,%eax
    41cd:	eb 29                	jmp    41f8 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1b8>
    41cf:	90                   	nop
    41d0:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			41d3: R_X86_64_32S	.rodata.str1.8+0x1380
    41d7:	31 c0                	xor    %eax,%eax
    41d9:	c7 05 00 00 00 00 01 	movl   $0x1,0x0(%rip)        # 41e3 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1a3>
    41e0:	00 00 00 
			41db: R_X86_64_PC32	.bss+0xd098
    41e3:	e8 00 00 00 00       	call   41e8 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1a8>
			41e4: R_X86_64_PLT32	printf-0x4
    41e8:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			41eb: R_X86_64_32S	.rodata.str1.8+0x13c0
    41ef:	31 c0                	xor    %eax,%eax
    41f1:	e8 00 00 00 00       	call   41f6 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1b6>
			41f2: R_X86_64_PLT32	printf-0x4
    41f6:	31 c0                	xor    %eax,%eax
    41f8:	48 83 c4 38          	add    $0x38,%rsp
    41fc:	5b                   	pop    %rbx
    41fd:	5d                   	pop    %rbp
    41fe:	41 5c                	pop    %r12
    4200:	41 5d                	pop    %r13
    4202:	41 5e                	pop    %r14
    4204:	41 5f                	pop    %r15
    4206:	c3                   	ret
    4207:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
    420e:	00 00 
    4210:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
			4213: R_X86_64_32S	.rodata.str1.8+0x1218
    4217:	31 c0                	xor    %eax,%eax
    4219:	e8 00 00 00 00       	call   421e <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1de>
			421a: R_X86_64_PLT32	printf-0x4
    421e:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    4223:	eb d3                	jmp    41f8 <_Z22cosmos_hv_subvert_hostP17cosmos_hypervisorj+0x1b8>
    4225:	83 c8 ff             	or     $0xffffffff,%eax
    4228:	c3                   	ret
    4229:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)

0000000000004230 <_Z27cosmos_hv_launch_test_guestP17cosmos_hypervisorj>:
