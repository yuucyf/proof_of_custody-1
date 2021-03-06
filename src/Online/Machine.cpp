/*
Copyright (c) 2017, The University of Bristol, Senate House, Tyndall Avenue, Bristol, BS8 1TH, United Kingdom.
Copyright (c) 2019, COSIC-KU Leuven, Kasteelpark Arenberg 10, bus 2452, B-3001 Leuven-Heverlee, Belgium.

All rights reserved
*/

#include <fstream>
using namespace std;

#include "Machine.h"

void Machine::SetUp_Memory(unsigned int whoami, const string& memtype) {
  char filename[2048];

  Mc.set_default(gfp(0));
  Ms.set_default(Share(whoami));
  Mr.set_default(Integer(0));

  // Initialize the global memory
  if (memtype.compare("old") == 0) {
    sprintf(filename, "Data/Memory-P%d", whoami);
    ifstream inpf(filename, ios::in | ios::binary);
    if (inpf.fail()) {
      throw file_error(filename);
    }
    // See below for ordering here
    inpf >> Mc >> Mr >> Ms;
    inpf.close();
  } else if (!(memtype.compare("empty") == 0)) {
    cerr << "Invalid memory argument" << endl;
    exit(1);
  }
}

void Machine::Dump_Memory(unsigned int whoami) {
  // Reduce memory size to speed up
  int max_size = 1 << 20;
  if (Mc.size() > max_size)
    Mc.resize(max_size);
  if (Ms.size() > max_size)
    Ms.resize(max_size);
  if (Mr.size() > max_size)
    Mr.resize(max_size);

  // Write out the memory to use next time
  char filename[2048];
  sprintf(filename, "Data/Memory-P%d", whoami);
  ofstream outf(filename, ios::out | ios::binary);
  // We do it in this order as this is needed by Script/test_result.py
  // and we do not want that Script to worry about sizes of Shares
  outf << Mc << Mr << Ms;
  outf.close();
}

void Machine::Init_OTI(unsigned int nthreads) {
  for (unsigned int i = 0; i < nthreads; i++) {
    pthread_mutex_init(&online_mutex[i], NULL);
    pthread_cond_init(&machine_ready[i], NULL);
    pthread_cond_init(&online_thread_ready[i], NULL);
    OTI[i].prognum = -2; // Dont do anything until we are ready
    OTI[i].finished = true;
    OTI[i].ready = false;
    OTI[i].arg = 0;
    // lock online thread for synchronization
    pthread_mutex_lock(&online_mutex[i]);
  }
}

void Machine::SetUp_Threads(unsigned int nthreads) {
  // Set up the thread data
  OTI.resize(nthreads);
  online_mutex.resize(nthreads);
  online_thread_ready.resize(nthreads);
  machine_ready.resize(nthreads);

  Init_OTI(nthreads);
}

void Machine::Synchronize() {
  for (unsigned int i = 0; i < OTI.size(); i++) {
    while (!OTI[i].ready) {
      fprintf(stderr, "Waiting for thread %d to be ready\n", i);
      pthread_cond_wait(&online_thread_ready[i], &online_mutex[i]);
    }
    pthread_mutex_unlock(&online_mutex[i]);
  }
}

void Machine::Signal_Ready(unsigned int num, bool flag) {
  pthread_mutex_lock(&online_mutex[num]);
  OTI[num].ready = flag;
  if (flag) {
    pthread_cond_signal(&online_thread_ready[num]);
  } else {
    OTI[num].prognum = -1;
    pthread_cond_signal(&machine_ready[num]);
  }
  pthread_mutex_unlock(&online_mutex[num]);
}

void Machine::Lock_Until_Ready(unsigned int num) {
  pthread_mutex_lock(&online_mutex[num]);
  if (!OTI[num].ready)
    pthread_cond_wait(&machine_ready[num], &online_mutex[num]);
  pthread_mutex_unlock(&online_mutex[num]);
}

int Machine::Pause_While_Nothing_To_Do(unsigned int num) {
  pthread_mutex_lock(&online_mutex[num]);
  if (OTI[num].prognum == -2) {
    pthread_cond_wait(&machine_ready[num], &online_mutex[num]);
  }
  int program = OTI[num].prognum;
  OTI[num].prognum = -2; // Reset it to run nothing next
  pthread_mutex_unlock(&online_mutex[num]);
  return program;
}

void Machine::Signal_Finished_Tape(unsigned int num) {
  pthread_mutex_lock(&online_mutex[num]);
  OTI[num].finished = true;
  pthread_cond_signal(&online_thread_ready[num]);
  pthread_mutex_unlock(&online_mutex[num]);
}

void Machine::Lock_Until_Finished_Tape(unsigned int num) {
  pthread_mutex_lock(&online_mutex[num]);
  if (!OTI[num].finished)
    pthread_cond_wait(&online_thread_ready[num], &online_mutex[num]);
  pthread_mutex_unlock(&online_mutex[num]);
}

void Machine::run() {
  // Initialise the timers
  timers.resize(N_TIMERS);
  for (unsigned int i = 0; i < N_TIMERS; i++) {
    timers[i].start();
  }

  // First go through the schedule and execute what is there
  bool flag = true;
  for (unsigned int i = 0; i < 1; i++) {
    Lock_Until_Finished_Tape(i);
  }

  // Tell all online threads to stop
  for (unsigned int i = 0; i < OTI.size(); i++) {
    Signal_Ready(i, false);
  }

  cerr << "Waiting for all clients to finish" << endl;
  // Wait until all clients have signed out
  for (unsigned int i = 0; i < OTI.size(); i++) {
    pthread_mutex_lock(&online_mutex[i]);
    OTI[i].ready = true;
    pthread_cond_signal(&machine_ready[i]);
    pthread_mutex_unlock(&online_mutex[i]);

    pthread_mutex_destroy(&online_mutex[i]);
    pthread_cond_destroy(&online_thread_ready[i]);
    pthread_cond_destroy(&machine_ready[i]);
  }
}

void Machine::start_timer(unsigned int i) {
  if (i >= N_TIMERS) {
    throw invalid_length();
  }
  timers[i].reset();
}

void Machine::stop_timer(unsigned int i) {
  if (i >= N_TIMERS) {
    throw invalid_length();
  }
  double time = timers[i].elapsed();
  printf(
      BENCH_TEXT_BOLD BENCH_COLOR_GREEN BENCH_MAGIC_START
      "{\"timer\":%u,\n"
      "  \"time\":{\"seconds\":%lf,\"ms\":%.4lf}\n"
      "}\n" BENCH_MAGIC_END BENCH_ATTR_RESET,
      i, time, (time * 1000));
}
