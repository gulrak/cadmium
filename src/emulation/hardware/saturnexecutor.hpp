// Auto-generated 2025-03-18T06:14:41 by /Users/schuemann/Development/c8/chiplet/defs/clarke.py

#pragma once

#include <chiplet/decoder/saturndecoder.hpp>

#include <array>
#include <cstdint>

namespace emu {

class SaturnExecutor : public SaturnDecoder {
public:
    SaturnExecutor() = default;
    ~SaturnExecutor() override = default;

    void op_akx_add_k_field_x_regpair12(const DecodeResult& decoded); // used
    void op_818txi_add_t_field_i_nzconst_4_x_reg(const DecodeResult& decoded);
    void op_809_add_a_p_1_c(const DecodeResult& decoded);
    void op_cx_add_a_x_regpair12(const DecodeResult& decoded); // used
    void op_818fxi_add_a_i_nzconst_4_x_reg(const DecodeResult& decoded); // used
    void op_16x_add_a_x_nzconst_4_d0(const DecodeResult& decoded); // used
    void op_17x_add_a_x_nzconst_4_d1(const DecodeResult& decoded); // used
    void op_0exy_and_x_field_y_regpair8(const DecodeResult& decoded); // used
    void op_0efy_and_a_y_regpair8(const DecodeResult& decoded); // used
    void op_8086xyy_brbc_x_const_4_a_yy_pcofs_5(const DecodeResult& decoded); // used
    void op_808axyy_brbc_x_const_4_c_yy_pcofs_5(const DecodeResult& decoded); // used
    void op_86xyy_brbc_x_const_4_st_yy_pcofs_3(const DecodeResult& decoded);
    void op_83zyy_brbc_z_hwflags_yy_pcofs_3(const DecodeResult& decoded);
    void op_8087xyy_brbs_x_const_4_a_yy_pcofs_5(const DecodeResult& decoded); // used
    void op_808bxyy_brbs_x_const_4_c_yy_pcofs_5(const DecodeResult& decoded);
    void op_87xyy_brbs_x_const_4_st_yy_pcofs_3(const DecodeResult& decoded);
    void op_5xx_brcc_xx_pcofs_1(const DecodeResult& decoded); // used
    void op_4xx_brcs_xx_pcofs_1(const DecodeResult& decoded); // used
    void op_89xyy_breq_1_p_x_const_4_yy_pcofs_3(const DecodeResult& decoded);
    void op_9tuyy_breq_t_field_u_regpair4_0_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_8auyy_breq_a_u_regpair4_0_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_9tuyy_brge_t_field_8_u_regpair4_8_yy_pcofs_3(const DecodeResult& decoded);
    void op_8buyy_brge_a_u_regpair4_8_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_9tuyy_brgt_t_field_8_u_regpair4_0_yy_pcofs_3(const DecodeResult& decoded);
    void op_8buyy_brgt_a_u_regpair4_0_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_9tuyy_brle_t_field_8_u_regpair4_12_yy_pcofs_3(const DecodeResult& decoded);
    void op_8buyy_brle_a_u_regpair4_12_yy_pcofs_3(const DecodeResult& decoded);
    void op_9tuyy_brlt_t_field_8_u_regpair4_4_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_8buyy_brlt_a_u_regpair4_4_yy_pcofs_3(const DecodeResult& decoded);
    void op_88xyy_brne_1_p_x_const_4_yy_pcofs_3(const DecodeResult& decoded);
    void op_9tuyy_brne_t_field_u_regpair4_4_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_8auyy_brne_a_u_regpair4_4_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_9tuyy_brnz_t_field_u_reg_12_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_8auyy_brnz_a_u_reg_12_yy_pcofs_3(const DecodeResult& decoded);
    void op_9tuyy_brz_t_field_u_reg_8_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_8auyy_brz_a_u_reg_8_yy_pcofs_3(const DecodeResult& decoded); // used
    void op_8083_buscb(const DecodeResult& decoded);
    void op_80b_buscc(const DecodeResult& decoded);
    void op_808d_buscd(const DecodeResult& decoded);
    void op_7xxx_call_3_xxx_pcofs_4(const DecodeResult& decoded); // used
    void op_8exxxx_call_4_xxxx_pcofs_6(const DecodeResult& decoded); // used
    void op_8fxxxxx_call_a_xxxxx_const_20(const DecodeResult& decoded); // used
    void op_apt_clr_p_field_8_t_reg(const DecodeResult& decoded); // used
    void op_dt_clr_a_t_reg(const DecodeResult& decoded); // used
    void op_08_clr_x_st(const DecodeResult& decoded);
    void op_8084x_clrb_x_const_4_a(const DecodeResult& decoded); // used
    void op_8088x_clrb_x_const_4_c(const DecodeResult& decoded);
    void op_84x_clrb_x_const_4_st(const DecodeResult& decoded); // used
    void op_82x_clrb_x_hwflags(const DecodeResult& decoded);
    void op_805_config(const DecodeResult& decoded);
    void op_0d_dec_1_p(const DecodeResult& decoded);
    void op_akw_dec_k_field_w_reg_12(const DecodeResult& decoded); // used
    void op_cw_dec_a_w_reg_12(const DecodeResult& decoded); // used
    void op_802_in_4_a(const DecodeResult& decoded);
    void op_803_in_4_c(const DecodeResult& decoded);
    void op_0c_inc_1_p(const DecodeResult& decoded);
    void op_bku_inc_k_field_u_reg_4(const DecodeResult& decoded); // used
    void op_eu_inc_a_u_reg_4(const DecodeResult& decoded); // used
    void op_808f_intoff(const DecodeResult& decoded); // used
    void op_8080_inton(const DecodeResult& decoded);
    void op_6xxx_jump_3_xxx_pcofs_1(const DecodeResult& decoded); // used
    void op_8cxxxx_jump_4_xxxx_pcofs_2(const DecodeResult& decoded);
    void op_808c_jump_a_a(const DecodeResult& decoded); // used
    void op_808e_jump_a_c(const DecodeResult& decoded);
    void op_81b2_jump_a_a(const DecodeResult& decoded); // used
    void op_81b3_jump_a_c(const DecodeResult& decoded); // used
    void op_8dxxxxx_jump_a_xxxxx_const_20(const DecodeResult& decoded);
    void op_2x_move_1_x_const_4_p(const DecodeResult& decoded);
    void op_80cx_move_1_p_c_x_const_4(const DecodeResult& decoded);
    void op_80dx_move_1_c_x_const_4_p(const DecodeResult& decoded);
    void op_19xx_move_2_xx_const_8_d0(const DecodeResult& decoded); // used
    void op_1dxx_move_2_xx_const_8_d1(const DecodeResult& decoded);
    void op_13x_move_4_x_daregpair_8(const DecodeResult& decoded);
    void op_1axxxx_move_4_xxxx_const_16_d0(const DecodeResult& decoded);
    void op_1exxxx_move_4_xxxx_const_16_d1(const DecodeResult& decoded);
    void op_1bxxxxx_move_5_xxxxx_const_20_d0(const DecodeResult& decoded); // used
    void op_1fxxxxx_move_5_xxxxx_const_20_d1(const DecodeResult& decoded); // used
    void op_15xi_move_i_nzconst_4_x_mrpair_8(const DecodeResult& decoded); // used
    void op_apz_move_p_field_8_z_regpair8_4(const DecodeResult& decoded); // used
    void op_15xt_move_t_field_x_mrpair_0(const DecodeResult& decoded); // used
    void op_81at0x_move_t_field_a_x_tempreg_0(const DecodeResult& decoded);
    void op_81at0x_move_t_field_c_x_tempreg_8(const DecodeResult& decoded);
    void op_81at1x_move_t_field_x_tempreg_0_a(const DecodeResult& decoded);
    void op_81at1x_move_t_field_x_tempreg_8_c(const DecodeResult& decoded);
    void op_81b4_move_a_pc_a(const DecodeResult& decoded); // used
    void op_81b5_move_a_pc_c(const DecodeResult& decoded); // used
    void op_806_move_a_id_c(const DecodeResult& decoded);
    void op_dz_move_a_z_regpair8_4(const DecodeResult& decoded); // used
    void op_14x_move_a_x_mrpair_0(const DecodeResult& decoded); // used
    void op_13x_move_a_x_daregpair_0(const DecodeResult& decoded); // used
    void op_81af0x_move_a_a_x_tempreg_0(const DecodeResult& decoded); // used
    void op_81af0x_move_a_c_x_tempreg_8(const DecodeResult& decoded); // used
    void op_81af1x_move_a_x_tempreg_0_a(const DecodeResult& decoded); // used
    void op_81af1x_move_a_x_tempreg_8_c(const DecodeResult& decoded); // used
    void op_14x_move_b_x_mrpair_8(const DecodeResult& decoded); // used
    void op_3ix_move_p_i_nzconst_4_x_varconst_i_c(const DecodeResult& decoded); // used
    void op_8082ix_move_p_i_nzconst_4_x_varconst_i_a(const DecodeResult& decoded); // used
    void op_10x_move_w_a_x_tempreg_0(const DecodeResult& decoded);
    void op_10x_move_w_c_x_tempreg_8(const DecodeResult& decoded); // used
    void op_11x_move_w_x_tempreg_0_a(const DecodeResult& decoded);
    void op_11x_move_w_x_tempreg_8_c(const DecodeResult& decoded); // used
    void op_09_move_x_st_c(const DecodeResult& decoded);
    void op_0a_move_x_c_st(const DecodeResult& decoded);
    void op_brv_neg_r_field_8_v_reg_8(const DecodeResult& decoded);
    void op_fv_neg_a_v_reg_8(const DecodeResult& decoded);
    void op_420_nop3(const DecodeResult& decoded); // used
    void op_6300_nop4(const DecodeResult& decoded); // used
    void op_64000_nop5(const DecodeResult& decoded); // used
    void op_brv_not_r_field_8_v_reg_12(const DecodeResult& decoded); // used
    void op_fv_not_a_v_reg_12(const DecodeResult& decoded);
    void op_0exy_or_x_field_y_regpair8_8(const DecodeResult& decoded);
    void op_0efy_or_a_y_regpair8_8(const DecodeResult& decoded);
    void op_800_out_s_c(const DecodeResult& decoded);
    void op_801_out_x_c(const DecodeResult& decoded);
    void op_07_pop_a_c(const DecodeResult& decoded);
    void op_06_push_a_c(const DecodeResult& decoded);
    void op_80a_reset(const DecodeResult& decoded);
    void op_01_ret(const DecodeResult& decoded);
    void op_8086x00_retbc_x_const_4_a(const DecodeResult& decoded);
    void op_808ax00_retbc_x_const_4_c(const DecodeResult& decoded);
    void op_86x00_retbc_x_const_4_st(const DecodeResult& decoded);
    void op_83z00_retbc_z_hwflags(const DecodeResult& decoded);
    void op_8087x00_retbs_x_const_4_a(const DecodeResult& decoded); // used
    void op_808bx00_retbs_x_const_4_c(const DecodeResult& decoded);
    void op_87x00_retbs_x_const_4_st(const DecodeResult& decoded);
    void op_500_retcc(const DecodeResult& decoded);
    void op_03_retclrc(const DecodeResult& decoded);
    void op_400_retcs(const DecodeResult& decoded); // used
    void op_89x00_reteq_1_p_x_const_4(const DecodeResult& decoded);
    void op_9tu00_reteq_t_field_u_regpair4_0(const DecodeResult& decoded);
    void op_8au00_reteq_a_u_regpair4_0(const DecodeResult& decoded);
    void op_9tu00_retge_t_field_8_u_regpair4_8(const DecodeResult& decoded);
    void op_8bu00_retge_a_u_regpair4_8(const DecodeResult& decoded);
    void op_9tu00_retgt_t_field_8_u_regpair4_0(const DecodeResult& decoded);
    void op_8bu00_retgt_a_u_regpair4_0(const DecodeResult& decoded); // used
    void op_0f_reti(const DecodeResult& decoded);
    void op_9tu00_retle_t_field_8_u_regpair4_12(const DecodeResult& decoded);
    void op_8bu00_retle_a_u_regpair4_12(const DecodeResult& decoded);
    void op_9tu00_retlt_t_field_8_u_regpair4_4(const DecodeResult& decoded);
    void op_8bu00_retlt_a_u_regpair4_4(const DecodeResult& decoded); // used
    void op_88x00_retne_1_p_x_const_4(const DecodeResult& decoded);
    void op_9tu00_retne_t_field_u_regpair4_4(const DecodeResult& decoded); // used
    void op_8au00_retne_a_u_regpair4_4(const DecodeResult& decoded);
    void op_9tu00_retnz_t_field_u_reg_12(const DecodeResult& decoded); // used
    void op_8au00_retnz_a_u_reg_12(const DecodeResult& decoded);
    void op_02_retsetc(const DecodeResult& decoded);
    void op_00_retsetxm(const DecodeResult& decoded);
    void op_9tu00_retz_t_field_u_reg_8(const DecodeResult& decoded);
    void op_8au00_retz_a_u_reg_8(const DecodeResult& decoded); // used
    void op_81x_rln_w_x_reg_0(const DecodeResult& decoded);
    void op_81x_rrn_w_x_reg_4(const DecodeResult& decoded);
    void op_80810_rsi(const DecodeResult& decoded);
    void op_8085x_setb_x_const_4_a(const DecodeResult& decoded);
    void op_8089x_setb_x_const_4_c(const DecodeResult& decoded);
    void op_85x_setb_x_const_4_st(const DecodeResult& decoded);
    void op_05_setdec(const DecodeResult& decoded);
    void op_04_sethex(const DecodeResult& decoded); // used
    void op_807_shutdn(const DecodeResult& decoded);
    void op_brw_sln_r_field_8_w_reg(const DecodeResult& decoded); // used
    void op_fw_sln_a_w_reg(const DecodeResult& decoded); // used
    void op_819rw_srb_r_field_w_reg(const DecodeResult& decoded);
    void op_819fw_srb_a_w_reg(const DecodeResult& decoded);
    void op_81w_srb_w_w_reg_12(const DecodeResult& decoded);
    void op_80e_sreq(const DecodeResult& decoded);
    void op_brw_srn_r_field_8_w_reg_4(const DecodeResult& decoded);
    void op_fw_srn_a_w_reg_4(const DecodeResult& decoded);
    void op_bty_sub_t_field_y_regpair8split(const DecodeResult& decoded);
    void op_818txi_sub_t_field_i_nzconst_4_x_reg_8(const DecodeResult& decoded);
    void op_ey_sub_a_y_regpair8split(const DecodeResult& decoded);
    void op_818fxi_sub_a_i_nzconst_4_x_reg_8(const DecodeResult& decoded); // used
    void op_18x_sub_a_x_nzconst_4_d0(const DecodeResult& decoded); // used
    void op_1cx_sub_a_x_nzconst_4_d1(const DecodeResult& decoded);
    void op_bty_subn_t_field_y_regpair4rev_12(const DecodeResult& decoded);
    void op_ey_subn_a_y_regpair4rev_12(const DecodeResult& decoded); // used
    void op_80fx_swap_1_p_c_x_const_4(const DecodeResult& decoded); // used
    void op_13x_swap_4_x_daregpair_10(const DecodeResult& decoded);
    void op_apz_swap_p_field_8_z_regpair4rev_12(const DecodeResult& decoded); // used
    void op_81at2x_swap_t_field_a_x_tempreg_0(const DecodeResult& decoded);
    void op_81at2x_swap_t_field_c_x_tempreg_8(const DecodeResult& decoded);
    void op_81b6_swap_a_a_pc(const DecodeResult& decoded);
    void op_81b7_swap_a_c_pc(const DecodeResult& decoded);
    void op_dz_swap_a_z_regpair4rev_12(const DecodeResult& decoded); // used
    void op_13x_swap_a_x_daregpair_2(const DecodeResult& decoded); // used
    void op_81af2x_swap_a_a_x_tempreg_0(const DecodeResult& decoded);
    void op_81af2x_swap_a_c_x_tempreg_8(const DecodeResult& decoded); // used
    void op_12x_swap_w_a_x_tempreg_0(const DecodeResult& decoded);
    void op_12x_swap_w_c_x_tempreg_8(const DecodeResult& decoded);
    void op_0b_swap_x_c_st(const DecodeResult& decoded);
    void op_804_uncnfg(const DecodeResult& decoded);
    void execute(const DecodeResult& decoded)
    {
        switch(decoded.oid) {
        case Opc_809_add_a_p_1_c: op_809_add_a_p_1_c(decoded); break;
        case Opc_cx_add_a_x_regpair12: op_cx_add_a_x_regpair12(decoded); break;
        case Opc_akx_add_k_field_x_regpair12: op_akx_add_k_field_x_regpair12(decoded); break;
        case Opc_818fxi_add_a_i_nzconst_4_x_reg: op_818fxi_add_a_i_nzconst_4_x_reg(decoded); break;
        case Opc_818txi_add_t_field_i_nzconst_4_x_reg: op_818txi_add_t_field_i_nzconst_4_x_reg(decoded); break;
        case Opc_16x_add_a_x_nzconst_4_d0: op_16x_add_a_x_nzconst_4_d0(decoded); break;
        case Opc_17x_add_a_x_nzconst_4_d1: op_17x_add_a_x_nzconst_4_d1(decoded); break;
        case Opc_0efy_and_a_y_regpair8: op_0efy_and_a_y_regpair8(decoded); break;
        case Opc_0exy_and_x_field_y_regpair8: op_0exy_and_x_field_y_regpair8(decoded); break;
        case Opc_8086xyy_brbc_x_const_4_a_yy_pcofs_5: op_8086xyy_brbc_x_const_4_a_yy_pcofs_5(decoded); break;
        case Opc_808axyy_brbc_x_const_4_c_yy_pcofs_5: op_808axyy_brbc_x_const_4_c_yy_pcofs_5(decoded); break;
        case Opc_86xyy_brbc_x_const_4_st_yy_pcofs_3: op_86xyy_brbc_x_const_4_st_yy_pcofs_3(decoded); break;
        case Opc_83zyy_brbc_z_hwflags_yy_pcofs_3: op_83zyy_brbc_z_hwflags_yy_pcofs_3(decoded); break;
        case Opc_8087xyy_brbs_x_const_4_a_yy_pcofs_5: op_8087xyy_brbs_x_const_4_a_yy_pcofs_5(decoded); break;
        case Opc_808bxyy_brbs_x_const_4_c_yy_pcofs_5: op_808bxyy_brbs_x_const_4_c_yy_pcofs_5(decoded); break;
        case Opc_87xyy_brbs_x_const_4_st_yy_pcofs_3: op_87xyy_brbs_x_const_4_st_yy_pcofs_3(decoded); break;
        case Opc_8086x00_retbc_x_const_4_a: op_8086x00_retbc_x_const_4_a(decoded); break;
        case Opc_808ax00_retbc_x_const_4_c: op_808ax00_retbc_x_const_4_c(decoded); break;
        case Opc_86x00_retbc_x_const_4_st: op_86x00_retbc_x_const_4_st(decoded); break;
        case Opc_83z00_retbc_z_hwflags: op_83z00_retbc_z_hwflags(decoded); break;
        case Opc_8087x00_retbs_x_const_4_a: op_8087x00_retbs_x_const_4_a(decoded); break;
        case Opc_808bx00_retbs_x_const_4_c: op_808bx00_retbs_x_const_4_c(decoded); break;
        case Opc_87x00_retbs_x_const_4_st: op_87x00_retbs_x_const_4_st(decoded); break;
        case Opc_5xx_brcc_xx_pcofs_1: op_5xx_brcc_xx_pcofs_1(decoded); break;
        case Opc_4xx_brcs_xx_pcofs_1: op_4xx_brcs_xx_pcofs_1(decoded); break;
        case Opc_500_retcc: op_500_retcc(decoded); break;
        case Opc_400_retcs: op_400_retcs(decoded); break;
        case Opc_89xyy_breq_1_p_x_const_4_yy_pcofs_3: op_89xyy_breq_1_p_x_const_4_yy_pcofs_3(decoded); break;
        case Opc_88xyy_brne_1_p_x_const_4_yy_pcofs_3: op_88xyy_brne_1_p_x_const_4_yy_pcofs_3(decoded); break;
        case Opc_89x00_reteq_1_p_x_const_4: op_89x00_reteq_1_p_x_const_4(decoded); break;
        case Opc_88x00_retne_1_p_x_const_4: op_88x00_retne_1_p_x_const_4(decoded); break;
        case Opc_8auyy_breq_a_u_regpair4_0_yy_pcofs_3: op_8auyy_breq_a_u_regpair4_0_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_breq_t_field_u_regpair4_0_yy_pcofs_3: op_9tuyy_breq_t_field_u_regpair4_0_yy_pcofs_3(decoded); break;
        case Opc_8auyy_brne_a_u_regpair4_4_yy_pcofs_3: op_8auyy_brne_a_u_regpair4_4_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brne_t_field_u_regpair4_4_yy_pcofs_3: op_9tuyy_brne_t_field_u_regpair4_4_yy_pcofs_3(decoded); break;
        case Opc_8auyy_brz_a_u_reg_8_yy_pcofs_3: op_8auyy_brz_a_u_reg_8_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brz_t_field_u_reg_8_yy_pcofs_3: op_9tuyy_brz_t_field_u_reg_8_yy_pcofs_3(decoded); break;
        case Opc_8auyy_brnz_a_u_reg_12_yy_pcofs_3: op_8auyy_brnz_a_u_reg_12_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brnz_t_field_u_reg_12_yy_pcofs_3: op_9tuyy_brnz_t_field_u_reg_12_yy_pcofs_3(decoded); break;
        case Opc_8buyy_brgt_a_u_regpair4_0_yy_pcofs_3: op_8buyy_brgt_a_u_regpair4_0_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brgt_t_field_8_u_regpair4_0_yy_pcofs_3: op_9tuyy_brgt_t_field_8_u_regpair4_0_yy_pcofs_3(decoded); break;
        case Opc_8buyy_brlt_a_u_regpair4_4_yy_pcofs_3: op_8buyy_brlt_a_u_regpair4_4_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brlt_t_field_8_u_regpair4_4_yy_pcofs_3: op_9tuyy_brlt_t_field_8_u_regpair4_4_yy_pcofs_3(decoded); break;
        case Opc_8buyy_brge_a_u_regpair4_8_yy_pcofs_3: op_8buyy_brge_a_u_regpair4_8_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brge_t_field_8_u_regpair4_8_yy_pcofs_3: op_9tuyy_brge_t_field_8_u_regpair4_8_yy_pcofs_3(decoded); break;
        case Opc_8buyy_brle_a_u_regpair4_12_yy_pcofs_3: op_8buyy_brle_a_u_regpair4_12_yy_pcofs_3(decoded); break;
        case Opc_9tuyy_brle_t_field_8_u_regpair4_12_yy_pcofs_3: op_9tuyy_brle_t_field_8_u_regpair4_12_yy_pcofs_3(decoded); break;
        case Opc_8au00_reteq_a_u_regpair4_0: op_8au00_reteq_a_u_regpair4_0(decoded); break;
        case Opc_9tu00_reteq_t_field_u_regpair4_0: op_9tu00_reteq_t_field_u_regpair4_0(decoded); break;
        case Opc_8au00_retne_a_u_regpair4_4: op_8au00_retne_a_u_regpair4_4(decoded); break;
        case Opc_9tu00_retne_t_field_u_regpair4_4: op_9tu00_retne_t_field_u_regpair4_4(decoded); break;
        case Opc_8au00_retz_a_u_reg_8: op_8au00_retz_a_u_reg_8(decoded); break;
        case Opc_9tu00_retz_t_field_u_reg_8: op_9tu00_retz_t_field_u_reg_8(decoded); break;
        case Opc_8au00_retnz_a_u_reg_12: op_8au00_retnz_a_u_reg_12(decoded); break;
        case Opc_9tu00_retnz_t_field_u_reg_12: op_9tu00_retnz_t_field_u_reg_12(decoded); break;
        case Opc_8bu00_retgt_a_u_regpair4_0: op_8bu00_retgt_a_u_regpair4_0(decoded); break;
        case Opc_9tu00_retgt_t_field_8_u_regpair4_0: op_9tu00_retgt_t_field_8_u_regpair4_0(decoded); break;
        case Opc_8bu00_retlt_a_u_regpair4_4: op_8bu00_retlt_a_u_regpair4_4(decoded); break;
        case Opc_9tu00_retlt_t_field_8_u_regpair4_4: op_9tu00_retlt_t_field_8_u_regpair4_4(decoded); break;
        case Opc_8bu00_retge_a_u_regpair4_8: op_8bu00_retge_a_u_regpair4_8(decoded); break;
        case Opc_9tu00_retge_t_field_8_u_regpair4_8: op_9tu00_retge_t_field_8_u_regpair4_8(decoded); break;
        case Opc_8bu00_retle_a_u_regpair4_12: op_8bu00_retle_a_u_regpair4_12(decoded); break;
        case Opc_9tu00_retle_t_field_8_u_regpair4_12: op_9tu00_retle_t_field_8_u_regpair4_12(decoded); break;
        case Opc_8083_buscb: op_8083_buscb(decoded); break;
        case Opc_80b_buscc: op_80b_buscc(decoded); break;
        case Opc_808d_buscd: op_808d_buscd(decoded); break;
        case Opc_804_uncnfg: op_804_uncnfg(decoded); break;
        case Opc_805_config: op_805_config(decoded); break;
        case Opc_807_shutdn: op_807_shutdn(decoded); break;
        case Opc_80a_reset: op_80a_reset(decoded); break;
        case Opc_80e_sreq: op_80e_sreq(decoded); break;
        case Opc_7xxx_call_3_xxx_pcofs_4: op_7xxx_call_3_xxx_pcofs_4(decoded); break;
        case Opc_8exxxx_call_4_xxxx_pcofs_6: op_8exxxx_call_4_xxxx_pcofs_6(decoded); break;
        case Opc_8fxxxxx_call_a_xxxxx_const_20: op_8fxxxxx_call_a_xxxxx_const_20(decoded); break;
        case Opc_dt_clr_a_t_reg: op_dt_clr_a_t_reg(decoded); break;
        case Opc_apt_clr_p_field_8_t_reg: op_apt_clr_p_field_8_t_reg(decoded); break;
        case Opc_8084x_clrb_x_const_4_a: op_8084x_clrb_x_const_4_a(decoded); break;
        case Opc_8088x_clrb_x_const_4_c: op_8088x_clrb_x_const_4_c(decoded); break;
        case Opc_84x_clrb_x_const_4_st: op_84x_clrb_x_const_4_st(decoded); break;
        case Opc_82x_clrb_x_hwflags: op_82x_clrb_x_hwflags(decoded); break;
        case Opc_08_clr_x_st: op_08_clr_x_st(decoded); break;
        case Opc_0d_dec_1_p: op_0d_dec_1_p(decoded); break;
        case Opc_cw_dec_a_w_reg_12: op_cw_dec_a_w_reg_12(decoded); break;
        case Opc_akw_dec_k_field_w_reg_12: op_akw_dec_k_field_w_reg_12(decoded); break;
        case Opc_802_in_4_a: op_802_in_4_a(decoded); break;
        case Opc_803_in_4_c: op_803_in_4_c(decoded); break;
        case Opc_0c_inc_1_p: op_0c_inc_1_p(decoded); break;
        case Opc_eu_inc_a_u_reg_4: op_eu_inc_a_u_reg_4(decoded); break;
        case Opc_bku_inc_k_field_u_reg_4: op_bku_inc_k_field_u_reg_4(decoded); break;
        case Opc_808f_intoff: op_808f_intoff(decoded); break;
        case Opc_8080_inton: op_8080_inton(decoded); break;
        case Opc_80810_rsi: op_80810_rsi(decoded); break;
        case Opc_808c_jump_a_a: op_808c_jump_a_a(decoded); break;
        case Opc_808e_jump_a_c: op_808e_jump_a_c(decoded); break;
        case Opc_81b2_jump_a_a: op_81b2_jump_a_a(decoded); break;
        case Opc_81b3_jump_a_c: op_81b3_jump_a_c(decoded); break;
        case Opc_81b4_move_a_pc_a: op_81b4_move_a_pc_a(decoded); break;
        case Opc_81b5_move_a_pc_c: op_81b5_move_a_pc_c(decoded); break;
        case Opc_81b6_swap_a_a_pc: op_81b6_swap_a_a_pc(decoded); break;
        case Opc_81b7_swap_a_c_pc: op_81b7_swap_a_c_pc(decoded); break;
        case Opc_6xxx_jump_3_xxx_pcofs_1: op_6xxx_jump_3_xxx_pcofs_1(decoded); break;
        case Opc_8cxxxx_jump_4_xxxx_pcofs_2: op_8cxxxx_jump_4_xxxx_pcofs_2(decoded); break;
        case Opc_8dxxxxx_jump_a_xxxxx_const_20: op_8dxxxxx_jump_a_xxxxx_const_20(decoded); break;
        case Opc_806_move_a_id_c: op_806_move_a_id_c(decoded); break;
        case Opc_dz_move_a_z_regpair8_4: op_dz_move_a_z_regpair8_4(decoded); break;
        case Opc_apz_move_p_field_8_z_regpair8_4: op_apz_move_p_field_8_z_regpair8_4(decoded); break;
        case Opc_dz_swap_a_z_regpair4rev_12: op_dz_swap_a_z_regpair4rev_12(decoded); break;
        case Opc_apz_swap_p_field_8_z_regpair4rev_12: op_apz_swap_p_field_8_z_regpair4rev_12(decoded); break;
        case Opc_14x_move_a_x_mrpair_0: op_14x_move_a_x_mrpair_0(decoded); break;
        case Opc_14x_move_b_x_mrpair_8: op_14x_move_b_x_mrpair_8(decoded); break;
        case Opc_15xt_move_t_field_x_mrpair_0: op_15xt_move_t_field_x_mrpair_0(decoded); break;
        case Opc_15xi_move_i_nzconst_4_x_mrpair_8: op_15xi_move_i_nzconst_4_x_mrpair_8(decoded); break;
        case Opc_13x_move_a_x_daregpair_0: op_13x_move_a_x_daregpair_0(decoded); break;
        case Opc_13x_move_4_x_daregpair_8: op_13x_move_4_x_daregpair_8(decoded); break;
        case Opc_13x_swap_a_x_daregpair_2: op_13x_swap_a_x_daregpair_2(decoded); break;
        case Opc_13x_swap_4_x_daregpair_10: op_13x_swap_4_x_daregpair_10(decoded); break;
        case Opc_10x_move_w_a_x_tempreg_0: op_10x_move_w_a_x_tempreg_0(decoded); break;
        case Opc_12x_swap_w_a_x_tempreg_0: op_12x_swap_w_a_x_tempreg_0(decoded); break;
        case Opc_10x_move_w_c_x_tempreg_8: op_10x_move_w_c_x_tempreg_8(decoded); break;
        case Opc_12x_swap_w_c_x_tempreg_8: op_12x_swap_w_c_x_tempreg_8(decoded); break;
        case Opc_11x_move_w_x_tempreg_0_a: op_11x_move_w_x_tempreg_0_a(decoded); break;
        case Opc_11x_move_w_x_tempreg_8_c: op_11x_move_w_x_tempreg_8_c(decoded); break;
        case Opc_81af0x_move_a_a_x_tempreg_0: op_81af0x_move_a_a_x_tempreg_0(decoded); break;
        case Opc_81af2x_swap_a_a_x_tempreg_0: op_81af2x_swap_a_a_x_tempreg_0(decoded); break;
        case Opc_81af0x_move_a_c_x_tempreg_8: op_81af0x_move_a_c_x_tempreg_8(decoded); break;
        case Opc_81af2x_swap_a_c_x_tempreg_8: op_81af2x_swap_a_c_x_tempreg_8(decoded); break;
        case Opc_81af1x_move_a_x_tempreg_0_a: op_81af1x_move_a_x_tempreg_0_a(decoded); break;
        case Opc_81af1x_move_a_x_tempreg_8_c: op_81af1x_move_a_x_tempreg_8_c(decoded); break;
        case Opc_81at0x_move_t_field_a_x_tempreg_0: op_81at0x_move_t_field_a_x_tempreg_0(decoded); break;
        case Opc_81at2x_swap_t_field_a_x_tempreg_0: op_81at2x_swap_t_field_a_x_tempreg_0(decoded); break;
        case Opc_81at0x_move_t_field_c_x_tempreg_8: op_81at0x_move_t_field_c_x_tempreg_8(decoded); break;
        case Opc_81at2x_swap_t_field_c_x_tempreg_8: op_81at2x_swap_t_field_c_x_tempreg_8(decoded); break;
        case Opc_81at1x_move_t_field_x_tempreg_0_a: op_81at1x_move_t_field_x_tempreg_0_a(decoded); break;
        case Opc_81at1x_move_t_field_x_tempreg_8_c: op_81at1x_move_t_field_x_tempreg_8_c(decoded); break;
        case Opc_3ix_move_p_i_nzconst_4_x_varconst_i_c: op_3ix_move_p_i_nzconst_4_x_varconst_i_c(decoded); break;
        case Opc_8082ix_move_p_i_nzconst_4_x_varconst_i_a: op_8082ix_move_p_i_nzconst_4_x_varconst_i_a(decoded); break;
        case Opc_19xx_move_2_xx_const_8_d0: op_19xx_move_2_xx_const_8_d0(decoded); break;
        case Opc_1axxxx_move_4_xxxx_const_16_d0: op_1axxxx_move_4_xxxx_const_16_d0(decoded); break;
        case Opc_1bxxxxx_move_5_xxxxx_const_20_d0: op_1bxxxxx_move_5_xxxxx_const_20_d0(decoded); break;
        case Opc_1dxx_move_2_xx_const_8_d1: op_1dxx_move_2_xx_const_8_d1(decoded); break;
        case Opc_1exxxx_move_4_xxxx_const_16_d1: op_1exxxx_move_4_xxxx_const_16_d1(decoded); break;
        case Opc_1fxxxxx_move_5_xxxxx_const_20_d1: op_1fxxxxx_move_5_xxxxx_const_20_d1(decoded); break;
        case Opc_2x_move_1_x_const_4_p: op_2x_move_1_x_const_4_p(decoded); break;
        case Opc_80cx_move_1_p_c_x_const_4: op_80cx_move_1_p_c_x_const_4(decoded); break;
        case Opc_80dx_move_1_c_x_const_4_p: op_80dx_move_1_c_x_const_4_p(decoded); break;
        case Opc_09_move_x_st_c: op_09_move_x_st_c(decoded); break;
        case Opc_0a_move_x_c_st: op_0a_move_x_c_st(decoded); break;
        case Opc_0b_swap_x_c_st: op_0b_swap_x_c_st(decoded); break;
        case Opc_fv_neg_a_v_reg_8: op_fv_neg_a_v_reg_8(decoded); break;
        case Opc_brv_neg_r_field_8_v_reg_8: op_brv_neg_r_field_8_v_reg_8(decoded); break;
        case Opc_420_nop3: op_420_nop3(decoded); break;
        case Opc_6300_nop4: op_6300_nop4(decoded); break;
        case Opc_64000_nop5: op_64000_nop5(decoded); break;
        case Opc_fv_not_a_v_reg_12: op_fv_not_a_v_reg_12(decoded); break;
        case Opc_brv_not_r_field_8_v_reg_12: op_brv_not_r_field_8_v_reg_12(decoded); break;
        case Opc_0efy_or_a_y_regpair8_8: op_0efy_or_a_y_regpair8_8(decoded); break;
        case Opc_0exy_or_x_field_y_regpair8_8: op_0exy_or_x_field_y_regpair8_8(decoded); break;
        case Opc_800_out_s_c: op_800_out_s_c(decoded); break;
        case Opc_801_out_x_c: op_801_out_x_c(decoded); break;
        case Opc_06_push_a_c: op_06_push_a_c(decoded); break;
        case Opc_07_pop_a_c: op_07_pop_a_c(decoded); break;
        case Opc_01_ret: op_01_ret(decoded); break;
        case Opc_02_retsetc: op_02_retsetc(decoded); break;
        case Opc_03_retclrc: op_03_retclrc(decoded); break;
        case Opc_0f_reti: op_0f_reti(decoded); break;
        case Opc_00_retsetxm: op_00_retsetxm(decoded); break;
        case Opc_81x_rln_w_x_reg_0: op_81x_rln_w_x_reg_0(decoded); break;
        case Opc_81x_rrn_w_x_reg_4: op_81x_rrn_w_x_reg_4(decoded); break;
        case Opc_8085x_setb_x_const_4_a: op_8085x_setb_x_const_4_a(decoded); break;
        case Opc_8089x_setb_x_const_4_c: op_8089x_setb_x_const_4_c(decoded); break;
        case Opc_85x_setb_x_const_4_st: op_85x_setb_x_const_4_st(decoded); break;
        case Opc_05_setdec: op_05_setdec(decoded); break;
        case Opc_04_sethex: op_04_sethex(decoded); break;
        case Opc_fw_sln_a_w_reg: op_fw_sln_a_w_reg(decoded); break;
        case Opc_brw_sln_r_field_8_w_reg: op_brw_sln_r_field_8_w_reg(decoded); break;
        case Opc_fw_srn_a_w_reg_4: op_fw_srn_a_w_reg_4(decoded); break;
        case Opc_brw_srn_r_field_8_w_reg_4: op_brw_srn_r_field_8_w_reg_4(decoded); break;
        case Opc_81w_srb_w_w_reg_12: op_81w_srb_w_w_reg_12(decoded); break;
        case Opc_819fw_srb_a_w_reg: op_819fw_srb_a_w_reg(decoded); break;
        case Opc_819rw_srb_r_field_w_reg: op_819rw_srb_r_field_w_reg(decoded); break;
        case Opc_bty_sub_t_field_y_regpair8split: op_bty_sub_t_field_y_regpair8split(decoded); break;
        case Opc_ey_sub_a_y_regpair8split: op_ey_sub_a_y_regpair8split(decoded); break;
        case Opc_bty_subn_t_field_y_regpair4rev_12: op_bty_subn_t_field_y_regpair4rev_12(decoded); break;
        case Opc_ey_subn_a_y_regpair4rev_12: op_ey_subn_a_y_regpair4rev_12(decoded); break;
        case Opc_818fxi_sub_a_i_nzconst_4_x_reg_8: op_818fxi_sub_a_i_nzconst_4_x_reg_8(decoded); break;
        case Opc_818txi_sub_t_field_i_nzconst_4_x_reg_8: op_818txi_sub_t_field_i_nzconst_4_x_reg_8(decoded); break;
        case Opc_18x_sub_a_x_nzconst_4_d0: op_18x_sub_a_x_nzconst_4_d0(decoded); break;
        case Opc_1cx_sub_a_x_nzconst_4_d1: op_1cx_sub_a_x_nzconst_4_d1(decoded); break;
        case Opc_80fx_swap_1_p_c_x_const_4: op_80fx_swap_1_p_c_x_const_4(decoded); break;
        default: break;
        }
    }
private:
    uint64_t _rA{};
    uint64_t _rB{};
    uint64_t _rC{};
    uint64_t _rD{};
    std::array<uint64_t,5> _rR{};
    std::array<uint32_t,8> _rRSTK{};
    uint16_t _rIN{};    // 10 bit
    uint16_t _rOUT{};   // 10 bit
    uint32_t _rPC{};
    uint32_t _rD0{};
    uint32_t _rD1{};
    uint16_t _rST{};
    uint8_t _rP{};
    uint8_t _rHS{};
    bool _rCarry{false};
};

}
