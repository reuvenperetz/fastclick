#ifndef CLICK_TSCCLOCK_HH
#define CLICK_TSCCLOCK_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/task.hh>
#include <click/sync.hh>
#include <click/timestamp.hh>
CLICK_DECLS

/* =c
 * TSCClock()
 * =s
 * TSC-based user clock
 * =d
 *
 * Use the TSC counter to provide a very fast timestamp method. It will
 * only be used after the clock system has proven to be stable enough,
 * on modern hardware it will be around 15 seconds.
 *
 * This will not work if your system does not have (grep /proc/cpuinfo) constant_tsc and nonstop_tsc.
 *
 * Click needs to be compiled with --enable-user-timestamp for this to be used.
 *
 */

class TSCClock : public Element { public:

  TSCClock() CLICK_COLD;

  const char *class_name() const        { return "TSCClock"; }
  const char *port_count() const        { return PORTS_0_0; }

  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage);

  void run_timer(Timer*);
  static void run_sync_timer(Timer*,void*);

  enum {h_now, h_now_steady, h_cycles, h_cycles_hz, h_phase};
  static String read_handler(Element *e, void *thunk);
  void add_handlers() CLICK_COLD;


  static int64_t now(void* user, bool steady);

private:
  int _verbose;
  bool _install;
  bool _nowait;
  bool _allow_offset;

  typedef enum {STABILIZE,SYNCHRONIZE,RUNNING} phase_t;
  phase_t _phase = STABILIZE;

  double tsc_freq;
  double avg_freq;
  int64_t last_timestamp[2] = {0};
  int64_t last_cycles[2] = {0};
  int64_t last_real[2] = {0};
  int64_t cycles_per_subsec_mult[2] = {0};

  struct state {
      int64_t local_tsc_offset;
      int local_synchronize_bad;
  };
  per_thread<state> tstate;

  unsigned current_clock = 0;

  int64_t cycles_per_subsec_shift = 0;

  //Base values for steady clock. Once set, it does not change
  int64_t steady_cycles_per_subsec_mult = 0;
  int64_t steady_timestamp[2] = {0};
  int64_t steady_cycle[2] = {0};

  int update_period_msec;
  int64_t update_period_subsec;
  Timer _correction_timer;

  //Alpha is the correction rate
  double alpha = 0.5;
  int alpha_stable = 0;
  int alpha_unstable = 0;

  //Average upper bound on the precision we should be able to get
  double max_precision;

  double total_error = 0;
  long n_ticks = 0;

  //Synchronizing stuffs
  atomic_uint32_t _synchronize_bad;
  atomic_uint32_t _synchronize_ok;
  Timer** _sync_timers;

  UserClock* _base;

  inline int64_t tick_to_subsec(int64_t delta, int64_t mult);
  inline int64_t tick_to_subsec_wall(int64_t delta);
  inline int64_t tick_to_subsec_steady(int64_t delta);
  inline int64_t subsec_to_tick(int64_t subsec, int64_t mult);
  inline int64_t freq_to_mult(int64_t freq);
  inline double delta_to_freq(int64_t tick, int64_t time);

  inline int64_t compute_now_steady() {
      return steady_timestamp[current_clock] + tick_to_subsec_steady(click_get_cycles() + tstate->local_tsc_offset - steady_cycle[current_clock]);
  }

  inline int64_t compute_now_wall(int clock) {
      return last_timestamp[clock] + tick_to_subsec_wall(click_get_cycles() + tstate->local_tsc_offset- last_cycles[clock]);
  }

  inline int64_t compute_now_wall() {
      return compute_now_wall(current_clock);
  }

  void initialize_clock();
  bool stabilize_tick();
  bool accumulate_tick(Timer*);

  int64_t get_real_timestamp(bool steady=false);
};

inline int64_t TSCClock::tick_to_subsec(int64_t delta, int64_t mult) {
    return ((delta * mult) >> cycles_per_subsec_shift);
}

inline int64_t TSCClock::subsec_to_tick(int64_t subsec, int64_t mult) {
    return (subsec << cycles_per_subsec_shift) / mult;
}

inline int64_t TSCClock::tick_to_subsec_wall(int64_t delta) {
    return tick_to_subsec(delta, cycles_per_subsec_mult[current_clock]);
}

inline int64_t TSCClock::tick_to_subsec_steady(int64_t delta) {
    return tick_to_subsec(delta, steady_cycles_per_subsec_mult);
}

inline int64_t TSCClock::freq_to_mult(int64_t freq) {
    return ((int64_t)Timestamp::subsec_per_sec << cycles_per_subsec_shift) / freq;
}
double TSCClock::delta_to_freq(int64_t tick, int64_t time) {
    return ((double)tick * Timestamp::subsec_per_sec) / time;
}


CLICK_ENDDECLS
#endif
