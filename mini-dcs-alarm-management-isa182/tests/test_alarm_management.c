/**
 * @file test_alarm_management.c
 * @brief ISA-18.2 Alarm Management Test Suite (40+ tests across L1-L7)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include "alarm_management_types.h"
#include "alarm_rationalization.h"
#include "alarm_engine.h"
#include "alarm_shelving_suppression.h"
#include "alarm_kpi_metrics.h"
#include "alarm_audit_trail.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %-55s", name); } while(0)
#define PASS() do { tests_passed++; printf(" PASS\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf(" FAIL: %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (cond) { PASS(); } else { FAIL(msg); } } while(0)

/* External declarations */
extern const char *isa18_priority_to_string(isa18_alarm_priority_t);
extern const char *isa18_alarm_type_to_string(isa18_alarm_type_t);
extern const char *isa18_alarm_state_to_string(isa18_alarm_state_t);
extern const char *isa18_lifecycle_phase_to_string(isa18_lifecycle_phase_t);
extern const char *isa18_severity_to_string(isa18_consequence_severity_t);
extern const char *isa18_urgency_to_string(isa18_urgency_t);
extern void isa18_alarm_config_init(isa18_alarm_config_t *, uint32_t, const char *);
extern void isa18_alarm_config_set_high(isa18_alarm_config_t *, double, double);
extern void isa18_alarm_config_set_low(isa18_alarm_config_t *, double, double);
extern uint32_t isa18_alarm_config_validate(const isa18_alarm_config_t *, char [][ISA18_MAX_MESSAGE_LEN], uint32_t);
extern bool isa18_alarm_type_is_analog(isa18_alarm_type_t);
extern bool isa18_alarm_type_is_discrete(isa18_alarm_type_t);
extern int isa18_priority_color_code(isa18_alarm_priority_t);
extern int isa18_compare_alarms_by_priority(const isa18_alarm_config_t *, const isa18_alarm_config_t *);
extern bool isa18_alarm_state_is_rtn(isa18_alarm_state_t);
extern bool isa18_alarm_state_is_active(isa18_alarm_state_t);
extern bool isa18_alarm_state_is_acknowledged(isa18_alarm_state_t);
extern uint32_t isa18_priority_to_numeric(isa18_alarm_priority_t);
extern time_t isa18_compute_on_delay_expiry(time_t, uint32_t);
extern time_t isa18_compute_off_delay_expiry(time_t, uint32_t);
extern bool isa18_alarm_can_activate(const isa18_alarm_config_t *);

/* L1 Definitions */
static void tL1_priority_string(void) {
    TEST("L1: Priority enum to string");
    CHECK(strcmp(isa18_priority_to_string(ISA18_PRIORITY_CRITICAL),"CRITICAL")==0,"CRITICAL");
    CHECK(strcmp(isa18_priority_to_string(ISA18_PRIORITY_HIGH),"HIGH")==0,"HIGH");
    CHECK(strcmp(isa18_priority_to_string(ISA18_PRIORITY_MEDIUM),"MEDIUM")==0,"MEDIUM");
    CHECK(strcmp(isa18_priority_to_string(ISA18_PRIORITY_LOW),"LOW")==0,"LOW");
}
static void tL1_type_string(void) {
    TEST("L1: Alarm type to string");
    CHECK(strcmp(isa18_alarm_type_to_string(ISA18_TYPE_HIGH),"HIGH")==0,"HIGH");
    CHECK(strcmp(isa18_alarm_type_to_string(ISA18_TYPE_RATE_OF_CHANGE),"RATE_OF_CHANGE")==0,"ROC");
    CHECK(strcmp(isa18_alarm_type_to_string(ISA18_TYPE_BAD_MEASUREMENT),"BAD_MEASUREMENT")==0,"BAD");
    CHECK(strcmp(isa18_alarm_type_to_string(ISA18_TYPE_DISCREPANCY),"DISCREPANCY")==0,"DISC");
}
static void tL1_state_string(void) {
    TEST("L1: Alarm state to string");
    CHECK(strcmp(isa18_alarm_state_to_string(ISA18_ALARM_STATE_NORMAL),"NORMAL")==0,"NORMAL");
    CHECK(strcmp(isa18_alarm_state_to_string(ISA18_ALARM_STATE_ACTIVE_UNACK),"ACTIVE_UNACK")==0,"UNACK");
    CHECK(strcmp(isa18_alarm_state_to_string(ISA18_ALARM_STATE_ACTIVE_ACK),"ACTIVE_ACK")==0,"ACK");
    CHECK(strcmp(isa18_alarm_state_to_string(ISA18_ALARM_STATE_RTN_UNACK),"RTN_UNACK")==0,"RTN");
    CHECK(strcmp(isa18_alarm_state_to_string(ISA18_ALARM_STATE_CLEARED),"CLEARED")==0,"CLEAR");
}
static void tL1_lifecycle_string(void) {
    TEST("L1: Lifecycle phase to string");
    CHECK(strstr(isa18_lifecycle_phase_to_string(ISA18_LIFECYCLE_PHILOSOPHY),"PHILOSOPHY")!=NULL,"A");
    CHECK(strstr(isa18_lifecycle_phase_to_string(ISA18_LIFECYCLE_RATIONALIZATION),"RATIONALIZATION")!=NULL,"C");
    CHECK(strstr(isa18_lifecycle_phase_to_string(ISA18_LIFECYCLE_AUDIT),"AUDIT")!=NULL,"I");
}
static void tL1_severity_string(void) {
    TEST("L1: Severity to string");
    CHECK(strcmp(isa18_severity_to_string(ISA18_SEVERITY_CRITICAL),"CRITICAL")==0,"CRIT");
    CHECK(strcmp(isa18_severity_to_string(ISA18_SEVERITY_MODERATE),"MODERATE")==0,"MOD");
}
static void tL1_config_init(void) {
    TEST("L1: Alarm config init");
    isa18_alarm_config_t a;
    isa18_alarm_config_init(&a, 42, "TIC-101");
    CHECK(a.alarm_id==42,"id"); CHECK(strcmp(a.tag_name,"TIC-101")==0,"tag");
    CHECK(a.alarm_type==ISA18_TYPE_HIGH,"type"); CHECK(a.priority==ISA18_PRIORITY_LOW,"pri");
    CHECK(a.current_state==ISA18_ALARM_STATE_NORMAL,"state"); CHECK(!a.is_rationalized,"rat");
    CHECK(!a.is_shelved,"shv"); CHECK(!a.is_suppressed,"sup"); CHECK(a.revision==1,"rev");
}
static void tL1_config_set_high(void) {
    TEST("L1: Set HIGH alarm config");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,1,"TAG");
    isa18_alarm_config_set_high(&a,100.0,2.0);
    CHECK(a.alarm_type==ISA18_TYPE_HIGH,"type"); CHECK(fabs(a.setpoint-100.0)<0.001,"sp");
    CHECK(fabs(a.deadband-2.0)<0.001,"db");
}
static void tL1_analog_discrete(void) {
    TEST("L1: Analog vs discrete type");
    CHECK(isa18_alarm_type_is_analog(ISA18_TYPE_HIGH),"HIGH analog");
    CHECK(isa18_alarm_type_is_analog(ISA18_TYPE_DEVIATION),"DEV analog");
    CHECK(isa18_alarm_type_is_discrete(ISA18_TYPE_STATE),"STATE discrete");
    CHECK(isa18_alarm_type_is_discrete(ISA18_TYPE_SCADA_OFFLINE),"SCADA discrete");
}
static void tL1_color_code(void) {
    TEST("L1: ISA-101 HMI color codes");
    CHECK(isa18_priority_color_code(ISA18_PRIORITY_CRITICAL)==0,"CRIT=Red");
    CHECK(isa18_priority_color_code(ISA18_PRIORITY_HIGH)==1,"HIGH=Magenta");
    CHECK(isa18_priority_color_code(ISA18_PRIORITY_MEDIUM)==2,"MED=Yellow");
    CHECK(isa18_priority_color_code(ISA18_PRIORITY_LOW)==3,"LOW=White");
}
static void tL1_delay_expiry(void) {
    TEST("L1: On/off delay expiry");
    CHECK(isa18_compute_on_delay_expiry(1000,5000)==1005,"on 5s");
    CHECK(isa18_compute_on_delay_expiry(1000,0)==1000,"on 0");
    CHECK(isa18_compute_off_delay_expiry(1000,3000)==1003,"off 3s");
}

/* L2 Core Concepts */
static void tL2_priority_corner(void) {
    TEST("L2: Priority matrix corners");
    CHECK(isa18_assign_priority_matrix(ISA18_SEVERITY_CRITICAL,ISA18_URGENCY_IMMEDIATE)==ISA18_PRIORITY_CRITICAL,"C+I=C");
    CHECK(isa18_assign_priority_matrix(ISA18_SEVERITY_CRITICAL,ISA18_URGENCY_NON_URGENT)==ISA18_PRIORITY_MEDIUM,"C+N=M");
    CHECK(isa18_assign_priority_matrix(ISA18_SEVERITY_MODERATE,ISA18_URGENCY_IMMEDIATE)==ISA18_PRIORITY_MEDIUM,"M+I=M");
    CHECK(isa18_assign_priority_matrix(ISA18_SEVERITY_MODERATE,ISA18_URGENCY_NON_URGENT)==ISA18_PRIORITY_LOW,"M+N=L");
}
static void tL2_justified_ok(void) {
    TEST("L2: Alarm justified - all criteria");
    CHECK(isa18_check_alarm_justified("Reactor runaway risk","Reduce feed + cooling",300,false),"justified");
}
static void tL2_justified_no_consequence(void) {
    TEST("L2: Not justified - no consequence");
    CHECK(!isa18_check_alarm_justified("","Do something",300,false),"not justified");
}
static void tL2_justified_eliminable(void) {
    TEST("L2: Not justified - eliminable by design");
    CHECK(!isa18_check_alarm_justified("Risk","Action",120,true),"not justified");
}
static void tL2_justified_insufficient_time(void) {
    TEST("L2: Not justified - time < 60s");
    CHECK(!isa18_check_alarm_justified("Risk","Action",30,false),"not justified");
}
static void tL2_state_utils(void) {
    TEST("L2: State utility functions");
    CHECK(isa18_alarm_state_is_active(ISA18_ALARM_STATE_ACTIVE_UNACK),"UNACK active");
    CHECK(isa18_alarm_state_is_active(ISA18_ALARM_STATE_ACTIVE_ACK),"ACK active");
    CHECK(!isa18_alarm_state_is_active(ISA18_ALARM_STATE_NORMAL),"NORMAL not active");
    CHECK(isa18_alarm_state_is_rtn(ISA18_ALARM_STATE_RTN_UNACK),"RTN is RTN");
    CHECK(isa18_alarm_state_is_acknowledged(ISA18_ALARM_STATE_ACTIVE_ACK),"ACK acked");
}
static void tL2_can_activate(void) {
    TEST("L2: Activation eligibility");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,1,"T");
    CHECK(!isa18_alarm_can_activate(&a),"unrat'd"); a.is_rationalized=true;
    CHECK(isa18_alarm_can_activate(&a),"rat'd"); a.is_shelved=true;
    CHECK(!isa18_alarm_can_activate(&a),"shelved"); a.is_shelved=false; a.is_suppressed=true;
    CHECK(!isa18_alarm_can_activate(&a),"suppressed"); a.is_suppressed=false; a.alarm_class=ISA18_CLASS_ALERT;
    CHECK(!isa18_alarm_can_activate(&a),"ALERT class");
}
static void tL2_shelve_approval(void) {
    TEST("L2: Shelve approval requirement");
    CHECK(isa18_check_shelve_approval_required(ISA18_PRIORITY_CRITICAL),"CRITICAL");
    CHECK(isa18_check_shelve_approval_required(ISA18_PRIORITY_HIGH),"HIGH");
    CHECK(!isa18_check_shelve_approval_required(ISA18_PRIORITY_LOW),"LOW");
}
static void tL2_suppression(void) {
    TEST("L2: Plant state suppression");
    uint32_t m=(1U<<3);
    CHECK(isa18_suppression_by_plant_state(m,(1U<<3)),"match");
    CHECK(!isa18_suppression_by_plant_state(m,(1U<<2)),"no match");
    CHECK(!isa18_suppression_by_plant_state(0,(1U<<3)),"zero mask");
    CHECK(!isa18_suppression_by_plant_state(m,0),"zero state");
}
static void tL2_ack(void) {
    TEST("L2: Operator acknowledge");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,1,"T");
    a.current_state=ISA18_ALARM_STATE_ACTIVE_UNACK;
    isa18_engine_acknowledge(&a,"OP_S",time(NULL));
    CHECK(a.current_state==ISA18_ALARM_STATE_ACTIVE_ACK,"acked");
}

/* L3 Engineering Structures */
static void tL3_mad_init(void) {
    TEST("L3: MAD init");
    isa18_master_alarm_database_t m;
    isa18_mad_init(&m,"Ref-Unit5","PHIL-001");
    CHECK(m.alarm_count==0,"count"); CHECK(strcmp(m.site_name,"Ref-Unit5")==0,"site");
    CHECK(m.revision==1,"rev");
}
static void tL3_mad_add_find(void) {
    TEST("L3: MAD add and find");
    isa18_master_alarm_database_t m; isa18_mad_init(&m,"A","P");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,0,"TIC-101"); isa18_mad_add_alarm(&m,&a);
    isa18_alarm_config_init(&a,0,"PIC-202"); isa18_mad_add_alarm(&m,&a);
    CHECK(m.alarm_count==2,"count");
    isa18_alarm_config_t *f=isa18_mad_find_alarm(&m,"PIC-202");
    CHECK(f!=NULL&&strcmp(f->tag_name,"PIC-202")==0,"find tag");
    f=isa18_mad_find_by_id(&m,1);
    CHECK(f!=NULL&&f->alarm_id==1,"find id");
}
static void tL3_mad_duplicate(void) {
    TEST("L3: MAD duplicate tag rejected");
    isa18_master_alarm_database_t m; isa18_mad_init(&m,"A","P");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,0,"SAME");
    CHECK(isa18_mad_add_alarm(&m,&a)==1,"first"); CHECK(isa18_mad_add_alarm(&m,&a)==0,"duplicate");
}
static void tL3_mad_remove(void) {
    TEST("L3: MAD remove");
    isa18_master_alarm_database_t m; isa18_mad_init(&m,"A","P");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,0,"A"); isa18_mad_add_alarm(&m,&a);
    isa18_alarm_config_init(&a,0,"B"); isa18_mad_add_alarm(&m,&a);
    isa18_alarm_config_init(&a,0,"C"); isa18_mad_add_alarm(&m,&a);
    CHECK(m.alarm_count==3,"3"); CHECK(isa18_mad_remove_alarm(&m,2),"remove B");
    CHECK(m.alarm_count==2,"2 left");
}
static void tL3_mad_priority_count(void) {
    TEST("L3: MAD by priority count");
    isa18_master_alarm_database_t m; isa18_mad_init(&m,"A","P");
    isa18_alarm_config_t a;
    for(int i=0;i<8;i++){isa18_alarm_config_init(&a,0,"");char t[16];snprintf(t,16,"T%d",i);strncpy(a.tag_name,t,63);a.priority=(isa18_alarm_priority_t)(i%4);isa18_mad_add_alarm(&m,&a);}
    uint32_t c,h,md,lw; isa18_mad_count_by_priority(&m,&c,&h,&md,&lw);
    CHECK(c==2&&h==2&&md==2&&lw==2,"even dist");
}
static void tL3_mad_coverage(void) {
    TEST("L3: MAD rationalization coverage");
    isa18_master_alarm_database_t m; isa18_mad_init(&m,"A","P");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,0,"1"); a.is_rationalized=true; isa18_mad_add_alarm(&m,&a);
    isa18_alarm_config_init(&a,0,"2"); a.is_rationalized=false; isa18_mad_add_alarm(&m,&a);
    CHECK(fabs(isa18_mad_calc_rationalization_coverage(&m)-50.0)<0.01,"50%");
}
static void tL3_state_machine_full(void) {
    TEST("L3: Full 5-state state machine path");
    isa18_alarm_state_t s;
    s=isa18_alarm_state_transition(ISA18_ALARM_STATE_NORMAL,true,false,false,false);
    CHECK(s==ISA18_ALARM_STATE_ACTIVE_UNACK,"N->UN");
    s=isa18_alarm_state_transition(ISA18_ALARM_STATE_ACTIVE_UNACK,true,true,false,false);
    CHECK(s==ISA18_ALARM_STATE_ACTIVE_ACK,"UN->A");
    s=isa18_alarm_state_transition(ISA18_ALARM_STATE_ACTIVE_ACK,false,false,false,false);
    CHECK(s==ISA18_ALARM_STATE_RTN_UNACK,"A->RN");
    s=isa18_alarm_state_transition(ISA18_ALARM_STATE_RTN_UNACK,false,true,false,false);
    CHECK(s==ISA18_ALARM_STATE_CLEARED,"RN->CL");
    s=isa18_alarm_state_transition(ISA18_ALARM_STATE_CLEARED,false,false,false,false);
    CHECK(s==ISA18_ALARM_STATE_NORMAL,"CL->N");
}
static void tL3_suppressed_no_trans(void) {
    TEST("L3: Suppressed/shelved stay NORMAL");
    CHECK(isa18_alarm_state_transition(ISA18_ALARM_STATE_NORMAL,true,false,true,false)==ISA18_ALARM_STATE_NORMAL,"sup");
    CHECK(isa18_alarm_state_transition(ISA18_ALARM_STATE_NORMAL,true,false,false,true)==ISA18_ALARM_STATE_NORMAL,"shv");
}
static void tL3_event_gen(void) {
    TEST("L3: Event record generation");
    isa18_alarm_event_t e; time_t n=time(NULL);
    isa18_engine_generate_event(&e,100,42,ISA18_ALARM_STATE_NORMAL,ISA18_ALARM_STATE_ACTIVE_UNACK,95.5,90.0,"OP_J",true,n);
    CHECK(e.event_id==100,"id"); CHECK(e.alarm_id==42,"aid");
    CHECK(fabs(e.process_value-95.5)<0.01,"pv"); CHECK(e.is_operator_action,"op");
}
static void tL3_rationalize_record(void) {
    TEST("L3: Rationalization record lifecycle");
    isa18_rationalization_record_t r; isa18_rationalization_init_record(&r,1,100,"TIC-101");
    CHECK(r.record_id==1&&r.alarm_id==100,"init");
    CHECK(isa18_rationalization_team_add_member(&r,"J Smith"),"add member");
    CHECK(r.team_count==1,"count");
    isa18_rationalization_set_outcome(&r,ISA18_CLASS_ALARM,ISA18_PRIORITY_HIGH,ISA18_SEVERITY_SEVERE,ISA18_URGENCY_PROMPT,true,"Valid");
    CHECK(r.priority==ISA18_PRIORITY_HIGH&&r.is_justified,"outcome");
}

/* L4 Engineering Laws */
static void tL4_safe_response(void) {
    TEST("L4: Max safe response time");
    CHECK(fabs(isa18_calc_max_safe_response_time(600,30,70)-500.0)<0.001,"600-30-70=500");
    CHECK(isa18_calc_max_safe_response_time(30,20,15)==0.0,"negative->0");
}
static void tL4_margin(void) {
    TEST("L4: Response time margin");
    CHECK(fabs(isa18_calc_response_time_margin(500,300)-0.4)<0.01,"(500-300)/500");
    CHECK(isa18_calc_response_time_margin(200,300)<0.0,"negative");
    CHECK(isa18_calc_response_time_margin(0,100)==-1.0,"no safe->-1");
}
static void tL4_config_validate(void) {
    TEST("L4: Config validation");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,1,"T");
    a.is_rationalized=true; a.deadband=-1.0; a.alarm_type=ISA18_TYPE_RATE_OF_CHANGE; a.rate_of_change_limit=0;
    char e[10][ISA18_MAX_MESSAGE_LEN]; uint32_t n=isa18_alarm_config_validate(&a,e,10);
    CHECK(n>=3,"3+ errors");
}
static void tL4_deadband_high_low(void) {
    TEST("L4: Deadband high and low");
    CHECK(fabs(isa18_apply_deadband(100,3,ISA18_TYPE_HIGH)-97.0)<0.001,"high 100-3=97");
    CHECK(fabs(isa18_apply_deadband(20,2,ISA18_TYPE_LOW)-22.0)<0.001,"low 20+2=22");
}
static void tL4_discrepancy_2oo3(void) {
    TEST("L4: 2oo3 discrepancy");
    CHECK(!isa18_check_discrepancy_alarm(50,50.5,49.8,2),"close");
    CHECK(isa18_check_discrepancy_alarm(50,80,49,2),"deviates");
    CHECK(!isa18_check_discrepancy_alarm(50,51,49,3),"in tolerance");
}
static void tL4_rationalize_apply(void) {
    TEST("L4: Apply rationalization to alarm");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,1,"TIC");
    isa18_rationalization_record_t r; isa18_rationalization_init_record(&r,1,1,"TIC");
    isa18_rationalization_set_outcome(&r,ISA18_CLASS_ALARM,ISA18_PRIORITY_HIGH,ISA18_SEVERITY_MAJOR,ISA18_URGENCY_RAPID,true,"Safe");
    r.max_safe_response_time_sec=180; isa18_rationalization_apply_to_alarm(&r,&a);
    CHECK(a.priority==ISA18_PRIORITY_HIGH,"pri"); CHECK(a.is_rationalized,"rat"); CHECK(a.time_to_respond_sec==180,"time");
}

/* L5 Algorithms */
static void tL5_chatter(void) {
    TEST("L5: Chattering detection");
    isa18_chattering_detector_t d; memset(&d,0,sizeof(d)); time_t t=time(NULL);
    CHECK(!isa18_kpi_detect_chattering(&d,t),"1"); CHECK(!isa18_kpi_detect_chattering(&d,t+5),"2");
    CHECK(isa18_kpi_detect_chattering(&d,t+10),"3->chatter"); CHECK(d.total_chattering_events==1,"count");
}
static void tL5_flood(void) {
    TEST("L5: Flood detection");
    isa18_alarm_flood_detector_t d; memset(&d,0,sizeof(d)); time_t t=time(NULL);
    bool triggered=false; for(uint32_t i=0;i<15;i++){if(isa18_flood_detector_update(&d,t+i,10))triggered=true;}
    CHECK(triggered,"triggered"); CHECK(d.is_flood_active,"active");
}
static void tL5_flood_end(void) {
    TEST("L5: Flood ends after timeout");
    isa18_alarm_flood_detector_t d; memset(&d,0,sizeof(d));
    d.window_start=1000; d.is_flood_active=true; d.alarms_in_window=12;
    isa18_flood_detector_check(&d,1601); CHECK(!d.is_flood_active,"ended");
}
static void tL5_peak_rate(void) {
    TEST("L5: Peak rate sliding window");
    isa18_history_entry_t h[30]; time_t b=1000000;
    for(int i=0;i<30;i++){h[i].timestamp=b+i*20;h[i].alarm_id=i+1;h[i].priority=ISA18_PRIORITY_LOW;h[i].transition_to=ISA18_ALARM_STATE_ACTIVE_UNACK;}
    CHECK(isa18_kpi_calc_peak_rate(h,30,b+600,600)>=15,">=15 in 600s");
}
static void tL5_apd(void) {
    TEST("L5: Alarms per day");
    isa18_kpi_counts_t k; time_t n=time(NULL); isa18_kpi_init(&k,n-3600,2);
    k.alarms_per_day=30; CHECK(fabs(isa18_kpi_calc_alarms_per_day(&k,n)-360.0)<2.0,"360/d");
}
static void tL5_sort(void) {
    TEST("L5: Priority sort");
    isa18_alarm_event_t e[4]; memset(e,0,sizeof(e));
    e[0].priority_number=4;e[0].timestamp=400;e[1].priority_number=1;e[1].timestamp=100;
    e[2].priority_number=3;e[2].timestamp=300;e[3].priority_number=1;e[3].timestamp=200;
    isa18_engine_priority_sort(e,4);
    CHECK(e[0].priority_number==1&&e[0].timestamp==100,"1st"); CHECK(e[1].priority_number==1,"2nd");
}
static void tL5_reset_chatter(void) {
    TEST("L5: Chattering reset");
    isa18_chattering_detector_t d; memset(&d,0,sizeof(d));
    d.is_chattering=true; d.transition_count=10; d.total_chattering_events=5;
    isa18_kpi_chattering_reset(&d);
    CHECK(!d.is_chattering,"flag"); CHECK(d.transition_count==0,"cnt"); CHECK(d.total_chattering_events==5,"cumul");
}
static void tL5_health(void) {
    TEST("L5: Health score bounds");
    isa18_kpi_counts_t k; isa18_kpi_init(&k,time(NULL)-86400,1);
    double h=isa18_kpi_overall_health_score(&k,time(NULL));
    CHECK(h>=0.0&&h<=100.0,"[0,100]");
}
static void tL5_eemua(void) {
    TEST("L5: EEMUA 191 benchmarks");
    isa18_kpi_counts_t k; isa18_kpi_init(&k,time(NULL)-86400,1);
    k.alarms_per_day=120;k.peak_10min_rate=5;k.stale_24h_count=0;k.chattering_alarms=0;
    k.avg_response_time_sec=30;k.alarm_rationalization_coverage=100;
    int s[7]; isa18_kpi_assess_eemua_benchmark(&k,time(NULL),s);
    CHECK(s[0]>=3,"APD ok"); CHECK(s[1]>=3,"peak ok"); CHECK(s[2]>=4,"stale excellent");
}
static void tL5_nuisance(void) {
    TEST("L5: Nuisance detection");
    CHECK(isa18_kpi_detect_nuisance_alarm(2,100,20),"fast ack+frequent shelve");
    CHECK(!isa18_kpi_detect_nuisance_alarm(30,100,2),"normal");
}

/* L6 Integration */
static void tL6_engine_scan(void) {
    TEST("L6: Full engine scan");
    isa18_alarm_config_t c[10]; isa18_alarm_system_runtime_t r; double p[10]; time_t n=time(NULL);
    for(uint32_t i=0;i<10;i++){char t[16];snprintf(t,16,"T-%02u",i+1);isa18_alarm_config_init(&c[i],i+1,t);isa18_alarm_config_set_high(&c[i],80+i*5,2);c[i].is_rationalized=true;c[i].priority=(isa18_alarm_priority_t)(i%4);p[i]=50;}
    isa18_engine_runtime_init(&r,c,10);
    CHECK(isa18_engine_scan(&r,p,10,n)==0,"all normal");
    p[0]=85;p[1]=90;p[2]=95;
    CHECK(isa18_engine_scan(&r,p,10,n+1)==3,"3 tripped"); CHECK(r.active_alarms==3,"3 active");
}
static void tL6_shelve_workflow(void) {
    TEST("L6: Shelving workflow");
    isa18_alarm_config_t a; isa18_alarm_config_init(&a,1,"T"); a.is_rationalized=true; a.priority=ISA18_PRIORITY_MEDIUM; a.current_state=ISA18_ALARM_STATE_ACTIVE_UNACK;
    isa18_alarm_shelve_t s[10]; uint32_t sc=0;
    CHECK(isa18_shelve_alarm(&a,s,&sc,10,"Maintenance","OP","SUP",3600,time(NULL))>0,"shelve");
    CHECK(a.is_shelved,"marked"); CHECK(sc==1,"1 rec");
    CHECK(isa18_unshelve_alarm(&a,s,&sc,"OP",time(NULL)),"unshelve");
    CHECK(!a.is_shelved&&sc==0,"cleared");
}
static void tL6_auto_unshelve(void) {
    TEST("L6: Auto unshelve expired");
    isa18_alarm_config_t a[2]; isa18_alarm_config_init(&a[0],1,"1");a[0].is_rationalized=true;a[0].is_shelved=true;a[0].shelve_expiry=1000;
    isa18_alarm_config_init(&a[1],2,"2");a[1].is_rationalized=true;
    isa18_alarm_shelve_t s[5];s[0].alarm_id=1;s[0].is_active=true;uint32_t sc=1;
    CHECK(isa18_auto_unshelve_expired(a,2,s,&sc,2000)==1,"1 auto"); CHECK(!a[0].is_shelved,"done");
}

/* L7 Applications */
static void tL7_audit_log(void) {
    TEST("L7: Audit log");
    isa18_audit_system_t au; isa18_audit_init(&au,"Ref-A");
    CHECK(isa18_audit_log_event(&au,"OP_S","ACK","ALARM",101,"UN","A","Temp ack","C-01",time(NULL))==1,"entry1");
    CHECK(au.entry_count==1,"1 entry");
}
static void tL7_audit_query_alarm(void) {
    TEST("L7: Audit query by alarm");
    isa18_audit_system_t au; isa18_audit_init(&au,"Q"); time_t n=time(NULL);
    isa18_audit_log_event(&au,"A","ACK","ALARM",1,"-","-","-","W",n);
    isa18_audit_log_event(&au,"B","SHV","ALARM",2,"-","-","-","W",n+1);
    isa18_audit_log_event(&au,"C","ACK","ALARM",1,"-","-","-","W",n+2);
    isa18_audit_entry_t r[10];
    CHECK(isa18_audit_query_by_alarm(&au,1,r,10)==2,"2 for alarm 1");
}
static void tL7_audit_csv(void) {
    TEST("L7: Audit CSV export");
    isa18_audit_entry_t e[1]; memset(e,0,sizeof(e)); e[0].entry_id=1; e[0].timestamp=1700000000; strncpy(e[0].actor_id,"OP_S",31);
    char csv[2048]; CHECK(isa18_audit_export_csv(e,1,csv,2048)>=1,"csv rows");
}
static void tL7_audit_chain(void) {
    TEST("L7: Chain integrity");
    isa18_audit_system_t au; isa18_audit_init(&au,"C"); time_t n=time(NULL);
    isa18_audit_log_event(&au,"A","X","Y",1,"-","-","-","-",n);
    isa18_audit_log_event(&au,"B","X","Y",2,"-","-","-","-",n+1);
    CHECK(isa18_audit_verify_chain(&au),"intact");
}
static void tL7_regulatory(void) {
    TEST("L7: Regulatory report");
    isa18_audit_system_t au; isa18_audit_init(&au,"R"); time_t n=time(NULL);
    isa18_audit_log_event(&au,"OP","ACK","ALARM",1,"-","-","Test","W",n);
    char rpt[4096]; CHECK(isa18_audit_generate_regulatory_report(&au,n-60,n+60,rpt,4096)>0,"report gen");
}
static void tL7_kpi_report(void) {
    TEST("L7: KPI report");
    isa18_kpi_counts_t k; time_t n=time(NULL); isa18_kpi_init(&k,n-3600,2);
    k.alarms_per_day=50; k.peak_10min_rate=5; k.active_alarm_count=3;
    char rpt[4096]; CHECK(isa18_kpi_generate_report(&k,n,rpt,4096)>0,"report");
}
static void tL7_priority_dist(void) {
    TEST("L7: Priority distribution");
    isa18_kpi_counts_t k; memset(&k,0,sizeof(k));
    k.critical_per_day=10;k.high_per_day=30;k.medium_per_day=120;k.low_per_day=40;
    double c,h,m,l; isa18_kpi_calc_priority_distribution(&k,&c,&h,&m,&l);
    CHECK(fabs(c-5)<0.5,"crit 5%"); CHECK(fabs(h-15)<0.5,"high 15%"); CHECK(fabs(m-60)<1,"med 60%"); CHECK(fabs(l-20)<0.5,"low 20%");
}

/* Main */
int main(void) {
    printf("\n============================================\n  ISA-18.2 ALARM MANAGEMENT TEST SUITE\n============================================\n\n");
    printf("[L1] Definitions\n"); tL1_priority_string(); tL1_type_string(); tL1_state_string(); tL1_lifecycle_string(); tL1_severity_string(); tL1_config_init(); tL1_config_set_high(); tL1_analog_discrete(); tL1_color_code(); tL1_delay_expiry();
    printf("\n[L2] Core Concepts\n"); tL2_priority_corner(); tL2_justified_ok(); tL2_justified_no_consequence(); tL2_justified_eliminable(); tL2_justified_insufficient_time(); tL2_state_utils(); tL2_can_activate(); tL2_shelve_approval(); tL2_suppression(); tL2_ack();
    printf("\n[L3] Engineering Structures\n"); tL3_mad_init(); tL3_mad_add_find(); tL3_mad_duplicate(); tL3_mad_remove(); tL3_mad_priority_count(); tL3_mad_coverage(); tL3_state_machine_full(); tL3_suppressed_no_trans(); tL3_event_gen(); tL3_rationalize_record();
    printf("\n[L4] Engineering Laws\n"); tL4_safe_response(); tL4_margin(); tL4_config_validate(); tL4_deadband_high_low(); tL4_discrepancy_2oo3(); tL4_rationalize_apply();
    printf("\n[L5] Algorithms/Methods\n"); tL5_chatter(); tL5_flood(); tL5_flood_end(); tL5_peak_rate(); tL5_apd(); tL5_sort(); tL5_reset_chatter(); tL5_health(); tL5_eemua(); tL5_nuisance();
    printf("\n[L6] Canonical Problems\n"); tL6_engine_scan(); tL6_shelve_workflow(); tL6_auto_unshelve();
    printf("\n[L7] Industrial Applications\n"); tL7_audit_log(); tL7_audit_query_alarm(); tL7_audit_csv(); tL7_audit_chain(); tL7_regulatory(); tL7_kpi_report(); tL7_priority_dist();
    printf("\n============================================\n  RESULTS: %d/%d passed, %d failed\n============================================\n\n", tests_passed, tests_run, tests_failed);
    return (tests_failed > 0) ? 1 : 0;
}