/******************************************************************************
* Copyright (c) Intel Corporation - All rights reserved.                      *
* This file is part of the LIBXSMM library.                                   *
*                                                                             *
* For information on the license, see the LICENSE file.                       *
* Further information: https://github.com/libxsmm/libxsmm/                    *
* SPDX-License-Identifier: BSD-3-Clause                                       *
******************************************************************************/
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/
#include "generator_common_x86.h"
#include "generator_gemm_common.h"
#include "generator_x86_instructions.h"
#include "generator_packed_gemm_avx_avx2_avx512.h"

LIBXSMM_API_INTERN void libxsmm_generator_packed_gemm_avx_avx2_avx512( libxsmm_generated_code*         io_generated_code,
                                                                       const libxsmm_gemm_descriptor*  i_xgemm_desc,
                                                                       const unsigned int              i_packed_width ) {
  unsigned int l_max_n_reg_block = 0;
  unsigned int l_m_block = 0;
  unsigned int l_m_remainder = 0;

  libxsmm_micro_kernel_config l_micro_kernel_config;
  libxsmm_loop_label_tracker l_loop_label_tracker;
  libxsmm_gp_reg_mapping l_gp_reg_mapping;

  /* select accumulator blocking */
  /* TODO: we could do more aggressive blockings if needed */
  if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) {
    l_max_n_reg_block = 28;
  } else {
    l_max_n_reg_block = 13;
  }

  /* define gp register mapping */
  libxsmm_reset_x86_gp_reg_mapping( &l_gp_reg_mapping );
  /* matching calling convention on Linux */
#if defined(_WIN32) || defined(__CYGWIN__)
  l_gp_reg_mapping.gp_reg_param_struct = LIBXSMM_X86_GP_REG_RCX;
#else /* match calling convention on Linux */
  l_gp_reg_mapping.gp_reg_param_struct = LIBXSMM_X86_GP_REG_RDI;
#endif
  l_gp_reg_mapping.gp_reg_a = l_gp_reg_mapping.gp_reg_param_struct;
  l_gp_reg_mapping.gp_reg_b = LIBXSMM_X86_GP_REG_RSI;
  l_gp_reg_mapping.gp_reg_c = LIBXSMM_X86_GP_REG_RDX;
  l_gp_reg_mapping.gp_reg_a_prefetch = LIBXSMM_X86_GP_REG_RCX;
  l_gp_reg_mapping.gp_reg_b_prefetch = LIBXSMM_X86_GP_REG_R8;
  l_gp_reg_mapping.gp_reg_mloop = LIBXSMM_X86_GP_REG_R12;
  l_gp_reg_mapping.gp_reg_nloop = LIBXSMM_X86_GP_REG_R13;
  l_gp_reg_mapping.gp_reg_kloop = LIBXSMM_X86_GP_REG_R14;
  l_gp_reg_mapping.gp_reg_help_0 = LIBXSMM_X86_GP_REG_R10;     /* this is the SIMD packed register loop */
  l_gp_reg_mapping.gp_reg_help_1 = LIBXSMM_X86_GP_REG_R11;     /* this is the gp register to load the AVX512 mask, if needed */
  l_gp_reg_mapping.gp_reg_help_2 = LIBXSMM_X86_GP_REG_UNDEF;
  l_gp_reg_mapping.gp_reg_help_3 = LIBXSMM_X86_GP_REG_UNDEF;
  l_gp_reg_mapping.gp_reg_help_4 = LIBXSMM_X86_GP_REG_UNDEF;
  l_gp_reg_mapping.gp_reg_help_5 = LIBXSMM_X86_GP_REG_UNDEF;

  /* define loop_label_tracker */
  libxsmm_reset_loop_label_tracker( &l_loop_label_tracker );

  /* define the micro kernel code gen properties */
  libxsmm_generator_gemm_init_micro_kernel_config( &l_micro_kernel_config, io_generated_code->arch, i_xgemm_desc, 0 );
  /* for this kernel we need to overwrite the A and B load instructions */
  if ( LIBXSMM_DATATYPE_F64 == LIBXSMM_GEMM_GETENUM_AB_COMMON_PREC( i_xgemm_desc->datatype )  ) {
    l_micro_kernel_config.b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
  } else {
    l_micro_kernel_config.b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
  }

  /* open asm */
  libxsmm_x86_instruction_open_stream_gemm( io_generated_code, &l_gp_reg_mapping, 0, i_xgemm_desc->prefetch );

  /* implementing load from struct */
  if ( ((LIBXSMM_GEMM_FLAG_USE_XGEMM_ABI & i_xgemm_desc->flags) == LIBXSMM_GEMM_FLAG_USE_XGEMM_ABI) ) {
    /* RDI holds the pointer to the struct, so lets first move this one into R15 */
    libxsmm_x86_instruction_alu_reg( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, l_gp_reg_mapping.gp_reg_param_struct, l_gp_reg_mapping.gp_reg_help_1 );
    /* A pointer */
    libxsmm_x86_instruction_alu_mem( io_generated_code, l_micro_kernel_config.alu_mov_instruction,
                                     l_gp_reg_mapping.gp_reg_help_1, LIBXSMM_X86_GP_REG_UNDEF, 0, 32, l_gp_reg_mapping.gp_reg_a, 0 );
    /* B pointer */
    libxsmm_x86_instruction_alu_mem( io_generated_code, l_micro_kernel_config.alu_mov_instruction,
                                     l_gp_reg_mapping.gp_reg_help_1, LIBXSMM_X86_GP_REG_UNDEF, 0, 64, l_gp_reg_mapping.gp_reg_b, 0 );
    /* C pointer */
    libxsmm_x86_instruction_alu_mem( io_generated_code, l_micro_kernel_config.alu_mov_instruction,
                                     l_gp_reg_mapping.gp_reg_help_1, LIBXSMM_X86_GP_REG_UNDEF, 0, 96, l_gp_reg_mapping.gp_reg_c, 0 );
    if ( i_xgemm_desc->prefetch != LIBXSMM_GEMM_PREFETCH_NONE ) {
      /* A prefetch pointer */
      libxsmm_x86_instruction_alu_mem( io_generated_code, l_micro_kernel_config.alu_mov_instruction,
                                       l_gp_reg_mapping.gp_reg_help_1, LIBXSMM_X86_GP_REG_UNDEF, 0, 56, l_gp_reg_mapping.gp_reg_a_prefetch, 0 );
      /* B prefetch pointer */
      libxsmm_x86_instruction_alu_mem( io_generated_code, l_micro_kernel_config.alu_mov_instruction,
                                       l_gp_reg_mapping.gp_reg_help_1, LIBXSMM_X86_GP_REG_UNDEF, 0, 88, l_gp_reg_mapping.gp_reg_b_prefetch, 0 );
    }
  } else {
#if 0
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_ILLEGAL_ABI );
    return;
#endif
  }

  /* some very simple m-blocking */
  if ( i_xgemm_desc->m >= 4 ) {
    l_m_block = 4;
    l_m_remainder = i_xgemm_desc->m % 4;
    l_max_n_reg_block = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) ? 6 : 2;
  } else {
    l_m_block = 0;
    l_m_remainder = i_xgemm_desc->m;
  }

  if ( l_m_block == 4 ) {
    /* m loop */
    libxsmm_x86_instruction_alu_imm( io_generated_code, l_micro_kernel_config.alu_mov_instruction, l_gp_reg_mapping.gp_reg_mloop, 0 );
    libxsmm_x86_instruction_register_jump_back_label( io_generated_code, &l_loop_label_tracker );
    libxsmm_x86_instruction_alu_imm( io_generated_code, l_micro_kernel_config.alu_add_instruction, l_gp_reg_mapping.gp_reg_mloop, l_m_block );

    /* m loop body */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_mloop( io_generated_code, &l_loop_label_tracker, &l_gp_reg_mapping, &l_micro_kernel_config,
                                                         i_xgemm_desc, i_packed_width, l_max_n_reg_block, l_m_block );

    /* close m loop */
    libxsmm_x86_instruction_alu_imm( io_generated_code, l_micro_kernel_config.alu_cmp_instruction, l_gp_reg_mapping.gp_reg_mloop, i_xgemm_desc->m-l_m_remainder );
    libxsmm_x86_instruction_jump_back_to_label( io_generated_code, l_micro_kernel_config.alu_jmp_instruction, &l_loop_label_tracker );
  }

  if ( l_m_remainder != 0 ) {
    if ( l_m_remainder == 3 ) {
      l_m_block = 3;
      l_max_n_reg_block = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) ? 8 : 3;
    } else if ( l_m_remainder == 2 ) {
      l_m_block = 2;
      l_max_n_reg_block = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) ? 12 : 5;
    } else {
      l_m_block = 1;
      l_max_n_reg_block = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) ? 28 : ((io_generated_code->arch >= LIBXSMM_X86_AVX2) ? 13 : 12 );
    }

    /* m loop body */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_mloop( io_generated_code, &l_loop_label_tracker, &l_gp_reg_mapping, &l_micro_kernel_config,
                                                         i_xgemm_desc, i_packed_width, l_max_n_reg_block, l_m_block );
  }

  /* close asm */
  libxsmm_x86_instruction_close_stream_gemm( io_generated_code, &l_gp_reg_mapping, 0, i_xgemm_desc->prefetch );
}

LIBXSMM_API_INTERN void libxsmm_generator_packed_gemm_avx_avx2_avx512_mloop( libxsmm_generated_code*            io_generated_code,
                                                                             libxsmm_loop_label_tracker*        io_loop_label_tracker,
                                                                             const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
                                                                             const libxsmm_micro_kernel_config* i_micro_kernel_config,
                                                                             const libxsmm_gemm_descriptor*     i_xgemm_desc,
                                                                             const unsigned int                 i_packed_width,
                                                                             const unsigned int                 i_max_n_reg_block,
                                                                             const unsigned int                 i_m_blocking ) {
  unsigned int l_n1_range = 0;
  unsigned int l_n2_range = 0;
  unsigned int l_n1_block = 0;
  unsigned int l_n2_block = 0;

  /* calculate the chunk size of current columns to work on */
  if ( libxsmm_compute_equalized_blocking( i_xgemm_desc->n, i_max_n_reg_block, &l_n1_range, &l_n1_block, &l_n2_range, &l_n2_block ) ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_N_BLOCK );
    return;
  }

  /* loop over n-blocks */
  if ( l_n1_block == i_xgemm_desc->n ) {
    /* no N loop at all */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                               i_packed_width, i_xgemm_desc->n, i_m_blocking );
  } else if ( (l_n1_range > 0) && (l_n2_range > 0) ) {
    /* reset n loop */
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_nloop, 0 );

    /* we have two ranges */
    /* first range */
    libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_nloop, l_n1_block );

    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                               i_packed_width, l_n1_block, i_m_blocking );

    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_nloop, l_n1_range );
    libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );

    /* second range */
    libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_nloop, l_n2_block );

    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                               i_packed_width, l_n2_block, i_m_blocking );

    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_nloop, i_xgemm_desc->n );
    libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );

    /* reset B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)i_xgemm_desc->n * i_packed_width * i_xgemm_desc->ldb *i_micro_kernel_config->datatype_size_in );

    /* reset C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)i_xgemm_desc->n * i_packed_width * i_xgemm_desc->ldc *i_micro_kernel_config->datatype_size_out );
  } else if ( (l_n1_range > 0) && (l_n2_range == 0) ) {
    /* reset n loop */
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_nloop, 0 );

    /* we have one range */
    libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_nloop, l_n1_block );

    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                               i_packed_width, l_n1_block, i_m_blocking );

    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_nloop, i_xgemm_desc->n );
    libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );

    /* reset B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)i_xgemm_desc->n * i_packed_width * i_xgemm_desc->ldb * i_micro_kernel_config->datatype_size_in );

    /* reset C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)i_xgemm_desc->n * i_packed_width * i_xgemm_desc->ldc * i_micro_kernel_config->datatype_size_out );
  } else {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_N_BLOCK );
    return;
  }

  /* advance A pointer */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_a,
                                   (long long)i_micro_kernel_config->datatype_size_in * i_packed_width * i_m_blocking);

  /* advance C pointer */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_c,
                                   (long long)i_micro_kernel_config->datatype_size_out * i_packed_width * i_m_blocking);

}

LIBXSMM_API_INTERN void libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop( libxsmm_generated_code*            io_generated_code,
                                                                             libxsmm_loop_label_tracker*        io_loop_label_tracker,
                                                                             const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
                                                                             const libxsmm_micro_kernel_config* i_micro_kernel_config,
                                                                             const libxsmm_gemm_descriptor*     i_xgemm_desc,
                                                                             const unsigned int                 i_packed_width,
                                                                             const unsigned int                 i_n_blocking,
                                                                             const unsigned int                 i_m_blocking ) {
  /* calculate how many iterations we need */
  unsigned int l_simd_packed_remainder = 0;
  unsigned int l_simd_packed_iters = 0;
  unsigned int l_simd_packed_width = 0;

  /* select simd packing width and accumulator blocking */
  if ( LIBXSMM_DATATYPE_F64 == LIBXSMM_GEMM_GETENUM_AB_COMMON_PREC( i_xgemm_desc->datatype )  ) {
    if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch < LIBXSMM_X86_AVX512_VL128_SKX ) ) {
      l_simd_packed_width = 2;
    } else if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL256_SKX ) && ( io_generated_code->arch < LIBXSMM_X86_AVX512_SKX ) ) {
      l_simd_packed_width = 4;
    } else if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) {
      l_simd_packed_width = 8;
    } else {
      l_simd_packed_width = 4;
    }
  } else {
    if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch < LIBXSMM_X86_AVX512_VL128_SKX ) ) {
      l_simd_packed_width = 4;
    } else if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL256_SKX ) && ( io_generated_code->arch < LIBXSMM_X86_AVX512_SKX ) ) {
      l_simd_packed_width = 8;
    } else if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) {
      l_simd_packed_width = 16;
    } else {
      l_simd_packed_width = 8;
    }
  }
  l_simd_packed_remainder = i_packed_width % l_simd_packed_width;
  l_simd_packed_iters = i_packed_width/l_simd_packed_width;

  /* check if we have a single SIMD divisor */
  if ( l_simd_packed_width == i_packed_width ) {
    /* run inner compute kernel */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop_simd_packed( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                                           i_packed_width, l_simd_packed_width, l_simd_packed_width, i_n_blocking, i_m_blocking );
  /* check if we have a perfect SIMD divisor */
  } else if ( l_simd_packed_remainder == 0 ) {
    /* initialize packed loop */
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_help_0, 0 );
    libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_help_0, l_simd_packed_width );

    /* run inner compute kernel */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop_simd_packed( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                                           i_packed_width, l_simd_packed_width, l_simd_packed_width, i_n_blocking, i_m_blocking );

    /* advance A pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_a,
                                     (long long)l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* advance B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* advance C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)l_simd_packed_width * i_micro_kernel_config->datatype_size_out );

    /* jump back to pack loop label */
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_help_0, i_packed_width );
    libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );

    /* reset A pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_a,
                                     (long long)l_simd_packed_iters * l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* reset B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)l_simd_packed_iters * l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* reset C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)l_simd_packed_iters * l_simd_packed_width * i_micro_kernel_config->datatype_size_out );
  /* we need masking and have less than SIMD width */
  } else if ( l_simd_packed_width > i_packed_width  ) {
    /* run remainder compute kernel */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop_simd_packed( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                                           i_packed_width, l_simd_packed_width, l_simd_packed_remainder, i_n_blocking, i_m_blocking );
  /* we need the general case */
  } else {
    /* initialize packed loop */
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_help_0, 0 );
    libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_help_0, l_simd_packed_width );

    /* run inner compute kernel */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop_simd_packed( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                                           i_packed_width, l_simd_packed_width, l_simd_packed_width, i_n_blocking, i_m_blocking );

    /* advance A pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_a,
                                     (long long)l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* advance B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* advance C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)l_simd_packed_width  * i_micro_kernel_config->datatype_size_out );

    /* jump back to pack loop label */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_cmp_instruction,
                                     i_gp_reg_mapping->gp_reg_help_0,
                                     (long long)i_packed_width - l_simd_packed_remainder );
    libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );

    /* run remainder compute kernel */
    libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop_simd_packed( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc,
                                                                           i_packed_width, l_simd_packed_width, l_simd_packed_remainder, i_n_blocking, i_m_blocking );

    /* reset A pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_a,
                                     (long long)l_simd_packed_iters * l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* reset B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)l_simd_packed_iters * l_simd_packed_width * i_micro_kernel_config->datatype_size_in );

    /* reset C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_sub_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)l_simd_packed_iters * l_simd_packed_width * i_micro_kernel_config->datatype_size_out );
  }

  /* advance B and C pointers if N is bigger than our register blocking */
  if ( i_xgemm_desc->n != i_n_blocking ) {
    /* advance B pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_b,
                                     (long long)i_n_blocking * i_packed_width * i_xgemm_desc->ldb * i_micro_kernel_config->datatype_size_in );

    /* advance C pointer */
    libxsmm_x86_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_c,
                                     (long long)i_n_blocking * i_packed_width * i_xgemm_desc->ldc * i_micro_kernel_config->datatype_size_out );
  }
}

LIBXSMM_API_INTERN void libxsmm_generator_packed_gemm_avx_avx2_avx512_load_C( libxsmm_generated_code*            io_generated_code,
                                                                              const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
                                                                              const libxsmm_micro_kernel_config* i_micro_kernel_config,
                                                                              const libxsmm_gemm_descriptor*     i_xgemm_desc,
                                                                              const unsigned int                 i_n_blocking,
                                                                              const unsigned int                 i_m_blocking,
                                                                              const unsigned int                 i_use_masking,
                                                                              const unsigned int                 i_packed_width ) {
  unsigned int l_n = 0;
  unsigned int l_m = 0;
  unsigned int l_mask_reg = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) )
                              ? LIBXSMM_X86_AVX512_MASK : 15;

  for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
    for ( l_m = 0; l_m < i_m_blocking; l_m++ ) {
      if (0 != (LIBXSMM_GEMM_FLAG_BETA_0 & i_xgemm_desc->flags)) { /* Beta=0 */
        libxsmm_x86_instruction_vec_compute_3reg( io_generated_code,
                                                  i_micro_kernel_config->vxor_instruction,
                                                  i_micro_kernel_config->vector_name,
                                                  (l_n*i_m_blocking) + l_m, (l_n*i_m_blocking) + l_m, (l_n*i_m_blocking) + l_m );
      } else {
        libxsmm_x86_instruction_unified_vec_move( io_generated_code,
                                                  i_micro_kernel_config->c_vmove_instruction,
                                                  i_gp_reg_mapping->gp_reg_c, LIBXSMM_X86_GP_REG_UNDEF, 0,
                                                  (l_m + l_n*i_xgemm_desc->ldc)*i_packed_width*i_micro_kernel_config->datatype_size_out,
                                                  i_micro_kernel_config->vector_name,
                                                  (l_n*i_m_blocking) + l_m, i_use_masking, l_mask_reg, 0 );
      }
    }
  }
}

LIBXSMM_API_INTERN void libxsmm_generator_packed_gemm_avx_avx2_avx512_store_C( libxsmm_generated_code*            io_generated_code,
                                                                               const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
                                                                               const libxsmm_micro_kernel_config* i_micro_kernel_config,
                                                                               const libxsmm_gemm_descriptor*     i_xgemm_desc,
                                                                               const unsigned int                 i_n_blocking,
                                                                               const unsigned int                 i_m_blocking,
                                                                               const unsigned int                 i_use_masking,
                                                                               const unsigned int                 i_packed_width ) {
  unsigned int l_n = 0;
  unsigned int l_m = 0;
  unsigned int l_mask_reg = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) )
                              ? LIBXSMM_X86_AVX512_MASK : 15;

  for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
    for ( l_m = 0; l_m < i_m_blocking; l_m++ ) {
      libxsmm_x86_instruction_unified_vec_move( io_generated_code,
                                                i_micro_kernel_config->c_vmove_instruction,
                                                i_gp_reg_mapping->gp_reg_c, LIBXSMM_X86_GP_REG_UNDEF, 0,
                                                (l_m + l_n*i_xgemm_desc->ldc)*i_packed_width*i_micro_kernel_config->datatype_size_out,
                                                i_micro_kernel_config->vector_name,
                                                (l_n*i_m_blocking) + l_m, i_use_masking, l_mask_reg, 1 );
    }
  }
}

LIBXSMM_API_INTERN void libxsmm_generator_packed_gemm_avx_avx2_avx512_kloop_simd_packed( libxsmm_generated_code*            io_generated_code,
                                                                                         libxsmm_loop_label_tracker*        io_loop_label_tracker,
                                                                                         const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
                                                                                         const libxsmm_micro_kernel_config* i_micro_kernel_config,
                                                                                         const libxsmm_gemm_descriptor*     i_xgemm_desc,
                                                                                         const unsigned int                 i_packed_width,
                                                                                         const unsigned int                 i_simd_packed_width,
                                                                                         const unsigned int                 i_simd_packed_valid,
                                                                                         const unsigned int                 i_n_blocking,
                                                                                         const unsigned int                 i_m_blocking ) {
  unsigned int l_n = 0;
  unsigned int l_m = 0;
  unsigned int l_use_masking = 0;
  unsigned int l_mask_reg = ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) )
                              ? LIBXSMM_X86_AVX512_MASK : 15;

  /* check if we need to compute a mask */
  if ( i_simd_packed_width > i_simd_packed_valid ) {
    /* on AVX512 we can use mask registers */
    if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX512_VL128_SKX ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) {
      libxsmm_generator_initialize_avx512_mask( io_generated_code, i_gp_reg_mapping->gp_reg_help_1, l_mask_reg, i_simd_packed_width - i_simd_packed_valid, (libxsmm_datatype)LIBXSMM_GEMM_GETENUM_AB_COMMON_PREC( i_xgemm_desc->datatype ) );
    } else {
      const char l_id = LIBXSMM_CAST_CHAR(128 > i_n_blocking ? ((int)i_n_blocking) : 127);
      unsigned char l_data[32] = { 0 };
      unsigned int l_count;

      if ( LIBXSMM_DATATYPE_F64 == LIBXSMM_GEMM_GETENUM_AB_COMMON_PREC( i_xgemm_desc->datatype ) ) {
        unsigned long long* l_i64_ptr = (unsigned long long*)l_data;
        for ( l_count = 0; l_count < 4; ++l_count ) {
          if ( l_count < i_simd_packed_valid ) {
            l_i64_ptr[l_count] = 0xffffffffffffffff;
          } else {
            l_i64_ptr[l_count] = 0x0;
          }
        }
      } else {
        unsigned int* l_i32_ptr = (unsigned int*)l_data;
        for ( l_count = 0; l_count < 8; ++l_count ) {
          if ( l_count < i_simd_packed_valid ) {
            l_i32_ptr[l_count] = 0xffffffff;
          } else {
            l_i32_ptr[l_count] = 0x0;
          }
        }
      }

      libxsmm_x86_instruction_full_vec_load_of_constants ( io_generated_code, l_data, &l_id, 'y', l_mask_reg );
    }
    l_use_masking = LIBXSMM_X86_AVX512_MASK;
  }

  /* load C accumulator */
  libxsmm_generator_packed_gemm_avx_avx2_avx512_load_C( io_generated_code, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc, i_n_blocking, i_m_blocking, l_use_masking, i_packed_width );

  /* k loop */
  libxsmm_generator_gemm_header_kloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, 0, 1 );

  /* full vector load of A */
  /* in case of masking we need to distinguish between AVX/AVX2 and AVX512 */
  for ( l_m = 0; l_m < i_m_blocking; ++l_m ) {
    libxsmm_x86_instruction_unified_vec_move( io_generated_code,
                                              i_micro_kernel_config->a_vmove_instruction,
                                              i_gp_reg_mapping->gp_reg_a, LIBXSMM_X86_GP_REG_UNDEF, 0,
                                              l_m * i_packed_width * i_micro_kernel_config->datatype_size_in,
                                              i_micro_kernel_config->vector_name,
                                              i_n_blocking*i_m_blocking + l_m, l_use_masking, l_mask_reg, 0 );
  }

  /* loop over the register block */
  for ( l_n = 0; l_n < i_n_blocking; ++l_n ) {
    libxsmm_x86_instruction_unified_vec_move( io_generated_code,
                                              i_micro_kernel_config->b_vmove_instruction,
                                              i_gp_reg_mapping->gp_reg_b, LIBXSMM_X86_GP_REG_UNDEF, 0,
                                              l_n * i_xgemm_desc->ldb * i_packed_width * i_micro_kernel_config->datatype_size_in,
                                              i_micro_kernel_config->vector_name,
                                              i_n_blocking*i_m_blocking + i_m_blocking, l_use_masking, l_mask_reg, 0 );
    if ( ( io_generated_code->arch >= LIBXSMM_X86_AVX2 ) && ( io_generated_code->arch <= LIBXSMM_X86_ALLFEAT ) ) {
      for ( l_m = 0; l_m < i_m_blocking; ++l_m ) {
        libxsmm_x86_instruction_vec_compute_3reg( io_generated_code,
                                                  i_micro_kernel_config->vmul_instruction,
                                                  i_micro_kernel_config->vector_name,
                                                  i_n_blocking*i_m_blocking + l_m,
                                                  i_n_blocking*i_m_blocking + i_m_blocking,
                                                  (l_n * i_m_blocking) + l_m );
      }
    } else if ( io_generated_code->arch == LIBXSMM_X86_AVX ) {
      for ( l_m = 0; l_m < i_m_blocking; ++l_m ) {
        libxsmm_x86_instruction_vec_compute_3reg( io_generated_code,
                                                  i_micro_kernel_config->vmul_instruction,
                                                  i_micro_kernel_config->vector_name,
                                                  i_n_blocking*i_m_blocking + l_m,
                                                  i_n_blocking*i_m_blocking + i_m_blocking,
                                                  i_n_blocking*i_m_blocking + i_m_blocking + 1 );
        libxsmm_x86_instruction_vec_compute_3reg( io_generated_code,
                                                  i_micro_kernel_config->vadd_instruction,
                                                  i_micro_kernel_config->vector_name,
                                                  i_n_blocking*i_m_blocking + i_m_blocking + 1,
                                                  (l_n * i_m_blocking) + l_m,
                                                  (l_n * i_m_blocking) + l_m );
      }
    } else {
      /* cannot happen */
    }
  }

  /* advance A pointer */
  libxsmm_x86_instruction_alu_imm( io_generated_code,
                                   i_micro_kernel_config->alu_add_instruction,
                                   i_gp_reg_mapping->gp_reg_a,
                                   (long long)i_packed_width * i_xgemm_desc->lda * i_micro_kernel_config->datatype_size_in );

  /* advance B pointer */
  libxsmm_x86_instruction_alu_imm( io_generated_code,
                                   i_micro_kernel_config->alu_add_instruction,
                                   i_gp_reg_mapping->gp_reg_b,
                                   (long long)i_packed_width * i_micro_kernel_config->datatype_size_in );

  /* close k loop */
  libxsmm_generator_gemm_footer_kloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc, 0, i_xgemm_desc->k, 0 );

  /* store C accumulator */
  libxsmm_generator_packed_gemm_avx_avx2_avx512_store_C( io_generated_code, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc, i_n_blocking, i_m_blocking, l_use_masking, i_packed_width );

  /* reset A pointer */
  libxsmm_x86_instruction_alu_imm( io_generated_code,
                                   i_micro_kernel_config->alu_sub_instruction,
                                   i_gp_reg_mapping->gp_reg_a,
                                   (long long)i_xgemm_desc->k * i_packed_width * i_xgemm_desc->lda * i_micro_kernel_config->datatype_size_in );

  /* reset B Pointer */
  libxsmm_x86_instruction_alu_imm( io_generated_code,
                                   i_micro_kernel_config->alu_sub_instruction,
                                   i_gp_reg_mapping->gp_reg_b,
                                   (long long)i_xgemm_desc->k * i_packed_width * i_micro_kernel_config->datatype_size_in );
}