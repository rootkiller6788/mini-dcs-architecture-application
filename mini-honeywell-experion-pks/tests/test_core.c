/**
 * @file test_core.c
 * @brief Comprehensive test suite for mini-honeywell-experion-pks
 * Covers L1-L5: System, C300, PID, CEE, HMI, Redundancy, CAB
 */
#include "../include/experion_system.h"
#include "../include/c300_controller.h"
#include "../include/control_blocks.h"
#include "../include/cee_execution.h"
#include "../include/hmiweb_display.h"
#include "../include/dcs_redundancy.h"
#include "../include/experion_cab_bulk.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>

static int passed=0,failed=0;
#define T(n) do{printf("  TEST %-35s ... ",n);fflush(stdout);}while(0)
#define P() do{printf("PASS\n");passed++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);failed++;return;}while(0)
#define CE(a,b) do{int ae=(int)(a),be=(int)(b);if(ae!=be){printf("FAIL: %d!=%d\n",ae,be);failed++;return;}}while(0)
#define CD(a,b,t) do{double ad=(a),bd=(b);if(fabs(ad-bd)>(t)){printf("FAIL: %f!=%f\n",ad,bd);failed++;return;}}while(0)

static void t_sys_init(void){T("system_init");ExperionSystem s;CE(experion_system_init(&s,"TEST",100),0);CE(s.system_id,100);CE(s.mode,XMODE_INITIALIZING);P();}

static void t_sys_null(void){T("system_null_guard");CE(experion_system_init(NULL,"t",1),-1);CE(experion_system_init((ExperionSystem*)0x1,NULL,1),-1);P();}
static void t_node_reg(void){T("node_registration");ExperionSystem s;experion_system_init(&s,"T",1);CE(experion_system_register_node(&s,10,EXN_NODE_ESVT),0);CE(s.domain.nodes_total,1);CE(experion_system_register_node(&s,20,EXN_NODE_C300),0);CE(s.domain.nodes_total,2);CE(experion_system_register_node(&s,10,EXN_NODE_EST),-1);P();}
static void t_activation(void){T("system_activation");ExperionSystem s;experion_system_init(&s,"T",1);experion_system_register_node(&s,100,EXN_NODE_ESVT);CE(experion_system_activate(&s),0);CE(s.mode,XMODE_RUN);P();}
static void t_clock(void){T("ptp_clock_offset");int64_t o=experion_clock_offset_ns(1000,1100,1200,1300);CE(o,0);o=experion_clock_offset_ns(1000,1150,1200,1250);CE(o,50);CE(experion_clock_offset_ns(0,1100,1200,1300),INT64_MAX);P();}
static void t_c300_init(void){T("c300_controller_init");C300Controller c;CE(c300_init(&c,1,"C300",100),0);CE(c.controller_id,1);P();}
static void t_c300_io(void){T("c300_io_config");C300Controller c;c300_init(&c,1,"C300",100);CE(c300_configure_io_slot(&c,0,C3IO_AI_16CH_420MA),0);CE(c.io_slots[0].channel_count,16);CE(c300_configure_channel(&c,0,0,"TIC001",0.0,200.0,"degC"),0);P();}
static void t_c300_scale(void){T("c300_scaling");CD(c300_scale_to_eu(12.0,4.0,20.0,0.0,200.0),100.0,0.01);CD(c300_scale_to_eu(-10.0,0.0,100.0,0.0,100.0),0.0,0.01);P();}
static void t_pid_init(void){T("pid_block_init");PIDControlBlock p;CE(pid_block_init(&p,1,"PIC001"),0);CE(p.state.mode,PID_MANUAL);P();}
static void t_pid_p(void){T("pid_p_only");PIDControlBlock p;pid_block_init(&p,1,"PIC001");pid_set_tuning(&p,2.0,0.0,0.0);pid_set_mode(&p,PID_AUTO);p.state.sp=50.0;double o;CE(pid_execute(&p,40.0,0.25,&o),0);CD(o,20.0,0.1);P();}
static void t_pid_i(void){T("pid_integral");PIDControlBlock p;pid_block_init(&p,1,"PIC001");pid_set_tuning(&p,1.0,10.0,0.0);pid_set_limits(&p,0.0,100.0,0.0,100.0);pid_set_mode(&p,PID_AUTO);p.state.sp=50.0;double o1,o2;pid_execute(&p,40.0,0.25,&o1);pid_execute(&p,40.0,0.25,&o2);if(o2<=o1){F("integral not accumulating");}P();}
static void t_pid_bumpless(void){T("pid_bumpless");PIDControlBlock p;pid_block_init(&p,1,"PIC001");pid_set_tuning(&p,1.0,10.0,1.0);pid_bumpless_transfer(&p,50.0);CD(p.state.op,50.0,0.01);P();}
static void t_pid_vel(void){T("pid_velocity");PIDControlBlock p;pid_block_init(&p,1,"PIC001");pid_set_tuning(&p,1.0,10.0,0.0);pid_set_mode(&p,PID_AUTO);p.state.sp=50.0;double d;CE(pid_execute_velocity(&p,40.0,0.25,&d),0);if(d<=0.0){F("delta not positive");}P();}
static void t_cascade(void){T("cascade_pair");CascadePair cp;cascade_pair_init(&cp,10,20);cascade_engage(&cp,true);double s;CE(cascade_calculate_sp(&cp,60.0,&s),0);CD(s,60.0,0.01);P();}
static void t_ff(void){T("feedforward");FeedforwardBlock ff;feedforward_init(&ff,0.5,0.0,0.0,0.25);double t;CE(feedforward_execute(&ff,10.0,50.0,&t),0);CD(t,55.0,0.1);P();}
static void t_ratio(void){T("ratio_control");RatioBlock r;ratio_block_init(&r,0.8);double s;ratio_execute(&r,100.0,&s);CD(s,80.0,0.01);P();}
static void t_split(void){T("split_range");SplitRangeBlock sr;split_range_init(&sr,2);split_range_set_breakpoint(&sr,0,0.0,50.0);split_range_set_breakpoint(&sr,1,50.0,100.0);double o[2];split_range_execute(&sr,25.0,o);CD(o[0],50.0,0.1);P();}
static void t_ovrd(void){T("override_selector");OverrideSelector os;override_selector_init(&os,OVRD_HIGH_SELECT,3);override_selector_set_input(&os,0,10.0);override_selector_set_input(&os,1,30.0);override_selector_set_input(&os,2,20.0);double s;override_selector_execute(&os,&s);CD(s,30.0,0.01);P();}
static void t_char(void){T("signal_characterizer");SignalCharacterizer sc;signal_char_init(&sc);signal_char_add_point(&sc,0.0,0.0);signal_char_add_point(&sc,50.0,25.0);signal_char_add_point(&sc,100.0,100.0);double y;signal_char_evaluate(&sc,50.0,&y);CD(y,25.0,0.01);P();}
static void t_rms(void){T("rms_bound");CD(cee_rms_bound(2),0.828,0.01);CD(cee_rms_bound(100),0.693,0.05);P();}
static void t_cee(void){T("cee_schedulability");CEEExecutionManager cee;cee_init(&cee,100);cee_add_phase(&cee,"REG",0,50000,CEE_CLASS_REGULATORY);cee_create_task(&cee,"T1",0,50,0,5000);cee_create_task(&cee,"T2",0,100,1,3000);CEESchedulabilityResult r;cee_analyze_schedulability(&cee,&r);CE(r.task_count,2);if(!r.schedulable){F("should be schedulable");}P();}
static void t_alarm(void){T("alarm_add_ack");HMIAlarmSummary a;memset(&a,0,sizeof(a));a.alarm_flood_threshold=10;HMIAlarmRecord r;memset(&r,0,sizeof(r));strcpy(r.tag,"TIC001");r.priority=HMI_ALARM_PRI_HIGH;r.requires_ack=true;r.state=HMI_ALARM_STATE_UNACK_ACTIVE;CE(alarm_add(&a,&r),0);CE(a.active_count,1);CE(alarm_acknowledge(&a,0,"OP1"),0);P();}
static void t_fp(void){T("faceplate_isa101");HMIFaceplate fp;memset(&fp,0,sizeof(fp));strcpy(fp.tag,"FIC001");fp.sp_hi_limit=100.0;fp.sp_lo_limit=0.0;fp.op_hi=100.0;fp.op_lo=0.0;CE(faceplate_update(&fp,50.0,60.0,70.0,PID_AUTO,XQUAL_GOOD),0);CD(fp.pv,50.0,0.01);CE(faceplate_set_sp(&fp,80.0,HMI_SEC_OPERATOR),0);CD(fp.sp,80.0,0.01);if(!isa101_verify_colors(&fp)){F("ISA-101 check");}P();}
static void t_red(void){T("redundancy_manager");RedundancyManager rm;CE(redundancy_init(&rm,100,RED_MOD_C300,10,20),0);CE(redundancy_set_role(&rm,RED_ROLE_PRIMARY),0);RedundancyHeartbeat hb;CE(redundancy_send_heartbeat(&rm,&hb),0);CE((int)hb.sender_role,(int)RED_ROLE_PRIMARY);RedundancyPairHealth h;redundancy_check_health(&rm,&h);CE((int)h,(int)RED_PAIR_HEALTHY);P();}
static void t_bump(void){T("bumpless_transfer");BumplessTransfer bt;CE(bumpless_transfer_init(&bt,2.0),0);bumpless_transfer_start(&bt,50.0,60.0);if(!bt.in_transition){F("not in transition");}double o=bumpless_transfer_update(&bt,60.0,0.5);CD(o,52.5,0.5);o=bumpless_transfer_update(&bt,60.0,2.0);CD(o,60.0,0.1);if(bt.in_transition){F("incomplete");}P();}
static void t_sil(void){T("sil_pfdavg");SILComplianceStatus sil;memset(&sil,0,sizeof(sil));CE(sil_calculate_pfdavg(&sil,87600.0,24.0,8760.0,0.05),0);if(sil.pfdavg_achieved<=0.0||sil.pfdavg_achieved>=1.0){F("PFDavg out of range");}bool c;sil_compliance_check(&sil,&c);if(!c){F("not compliant");}P();}
static void t_ma(void){T("moving_average");CABMovingAverage ma;CE(cab_moving_average_init(&ma,3),0);double a;cab_moving_average_update(&ma,10.0,&a);CD(a,10.0,0.01);cab_moving_average_update(&ma,20.0,&a);CD(a,15.0,0.01);free(ma.buffer);P();}
static void t_poly(void){T("polynomial_horner");CABPolynomial poly;double cf[]={1.0,2.0,3.0};cab_polynomial_init(&poly,2,cf);CD(cab_polynomial_eval(&poly,2.0),17.0,0.01);P();}
static void t_db(void){T("deadband");CABDeadband db;cab_deadband_init(&db,1.0,0.5);CD(cab_deadband_update(&db,5.0),5.0,0.01);CD(cab_deadband_update(&db,5.5),5.0,0.01);CD(cab_deadband_update(&db,4.0),5.0,0.01);CD(cab_deadband_update(&db,3.0),3.0,0.01);P();}
static void t_rlim(void){T("rate_limiter");CABRateLimiter rl;cab_rate_limiter_init(&rl,2.0,2.0);CD(cab_rate_limiter_update(&rl,10.0,1.0),10.0,0.01);CD(cab_rate_limiter_update(&rl,20.0,2.0),12.0,0.1);P();}


int main(void){
    printf("=== mini-honeywell-experion-pks Test Suite ===\n\n");
    printf("L1 - System Definitions:\n");t_sys_init();t_sys_null();t_node_reg();t_activation();t_clock();
    printf("\nL2 - C300 Controller:\n");t_c300_init();t_c300_io();t_c300_scale();
    printf("\nL5 - PID Algorithms:\n");t_pid_init();t_pid_p();t_pid_i();t_pid_bumpless();t_pid_vel();
    printf("\nL2 - Cascade/FF/Ratio/Split/Override:\n");t_cascade();t_ff();t_ratio();t_split();t_ovrd();t_char();
    printf("\nL5 - CEE Scheduling (Liu & Layland RMS):\n");t_rms();t_cee();
    printf("\nL2 - HMI Alarms & ISA-101:\n");t_alarm();t_fp();
    printf("\nL2/L4 - Redundancy & SIL (IEC 61508):\n");t_red();t_bump();t_sil();
    printf("\nL5 - CAB Utilities:\n");t_ma();t_poly();t_db();t_rlim();
    printf("\n=== %d passed, %d failed ===\n",passed,failed);
    return failed>0?1:0;
}
