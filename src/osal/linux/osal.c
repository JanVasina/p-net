/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2018 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

//#define _GNU_SOURCE

#include <osal.h>
#include <options.h>
#include <log.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "utils.h"
#include "config.h"
#include "i2c_led.h"
#include "osal.h"
#include "plc_memory.h"

#define USE_RPMALLOC

#define MAX_STATIC_BUFS 10 
#define BUF_SIZE        (2048 - sizeof(uint32_t))

#define USECS_PER_SEC     (1 * 1000 * 1000)
#define NSECS_PER_SEC     (1 * 1000 * 1000 * 1000)

//////////////////////////////////////////////////////////////////////////
// static functions prototypes
// 
static void log_sys_cmd(app_data_t *p_appdata, const char *cmd);
static void set_ip_address_to_interface(app_data_t *p_appdata, uint32_t ipaddr);

//////////////////////////////////////////////////////////////////////////
// static variables
// 
static os_mutex_t *log_mutex;
static os_mutex_t *pbuf_mutex;
typedef struct pbuf
{
  uint32_t in_use;
  uint8_t  data[BUF_SIZE];
} pbuf_t;

static pbuf_t bufs[MAX_STATIC_BUFS];

void os_log(int type, const char *fmt, ...)
{
  char   timestamp[16];
  char   info[1024];
  char   time_info[128];
  size_t info_len;
  time_t rawtime;
  struct tm *timestruct;

  os_mutex_lock(log_mutex);
  time(&rawtime);
  timestruct = localtime(&rawtime);
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timestruct);

  va_list list;

  switch (LOG_LEVEL_GET(type))
  {
  case LOG_LEVEL_DEBUG:
    snprintf(time_info, sizeof(time_info) - 1, "%s [DEBUG] ", timestamp);
    break;
  case LOG_LEVEL_INFO:
    snprintf(time_info, sizeof(time_info) - 1, "%s [" ANSI_COLOR_GREEN "INFO " ANSI_COLOR_RESET "] ", timestamp);
    break;
  case LOG_LEVEL_WARNING:
    snprintf(time_info, sizeof(time_info) - 1, "%s [" ANSI_COLOR_MAGENTA "WARN " ANSI_COLOR_RESET "] ", timestamp);
    break;
  case LOG_LEVEL_ERROR:
    snprintf(time_info, sizeof(time_info) - 1, "%s [" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] ", timestamp);
    break;
  default: 
    time_info[0] = '\0';
    break;
  }

  va_start(list, fmt);
  info_len = vsnprintf(info, sizeof(info)-1, fmt, list);
  va_end(list);

  fputs(time_info, stdout);
  if(info_len > 0)
  {
    fputs(info, stdout);
  }
  fflush(stdout);

  if(log_to_file)
  {
    bool b_flush = false;
    FILE *fp = fopen(LOG_FILE_NAME, "at");
    if (fp != NULL)
    {
      size_t time_info_len;
      switch (LOG_LEVEL_GET(type))
      {
      case LOG_LEVEL_DEBUG:
        time_info_len = snprintf(time_info, sizeof(time_info) - 1, "%s [DEBUG] ", timestamp);
        break;
      case LOG_LEVEL_INFO:
        time_info_len = snprintf(time_info, sizeof(time_info) - 1, "%s [INFO ] ", timestamp);
        break;
      case LOG_LEVEL_WARNING:
        time_info_len = snprintf(time_info, sizeof(time_info) - 1, "%s [WARN ] ", timestamp);
        b_flush = true;
        break;
      case LOG_LEVEL_ERROR:
        time_info_len = snprintf(time_info, sizeof(time_info) - 1, "%s [ERROR] ", timestamp);
        b_flush = true;
        break;
      default:
        time_info[0] = '\0';
        time_info_len = 0;
        break;
      }
      if (time_info_len > 0)
      {
        fwrite(time_info, time_info_len, 1, fp);
      }
      if (info_len > 0)
      {
        fwrite(info, info_len, 1, fp);
      }
      fclose(fp);
      if(b_flush == true)
      {
        system("sync");
      }
    }
  }

  os_mutex_unlock(log_mutex);
}

void os_init(void *arg)
{
  app_data_t *p_appdata = (app_data_t *)arg;
  rpmalloc_initialize();
  if (p_appdata->arguments.use_led != 0)
  {
    int i2c_file = open_led();
    if ((i2c_file < 0) && (p_appdata->arguments.verbosity > 0))
    {
      os_log(LOG_LEVEL_ERROR, "Could not open LED: %s\r\n", strerror(errno));
    }
    p_appdata->i2c_file = i2c_file;
  }
  memset(bufs, 0, sizeof(bufs));
  log_mutex = os_mutex_create();
  pbuf_mutex = os_mutex_create();
}

void os_exit(void *arg)
{
  app_data_t *p_appdata = (app_data_t *)arg;
  if (p_appdata->arguments.use_led != 0)
  {
    close_led(p_appdata->i2c_file);
  }
  p_appdata->i2c_file = 0;
  set_ip_address_to_interface(p_appdata, __builtin_bswap32(p_appdata->def_ip));
  //remove_last_added_ip_address_from_interface(p_appdata);
  os_mutex_destroy(log_mutex);
  log_mutex = NULL;
  os_mutex_destroy(pbuf_mutex);
  pbuf_mutex = NULL;
  rpmalloc_finalize();
}

void *os_malloc(size_t size)
{
#ifdef USE_RPMALLOC
  void *ptr = rpmalloc(size);
#else
  void *ptr = malloc(size);
#endif // USE_RPMALLOC
  if(ptr == NULL)
  {
    os_log(LOG_LEVEL_ERROR,
           "PNET: Not enough memory (wanted size %u).\nProgram is terminated\n",
           size);
    exit(EXIT_CODE_ERROR);
  }
  return ptr;
}

void os_free(void *ptr)
{
#ifdef USE_RPMALLOC
  rpfree(ptr);
#else
  free(ptr);
#endif
}

os_thread_t *os_thread_create(const char *name, int priority,
                              int stacksize, void *(*entry) (void *arg), void *arg)
{
  int result;
  pthread_t *thread = os_malloc(sizeof(*thread));
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + stacksize);

#if defined (USE_SCHED_FIFO)
  if(priority > 0)
  {
    CC_STATIC_ASSERT(_POSIX_THREAD_PRIORITY_SCHEDULING > 0);
    struct sched_param param = { .sched_priority = priority };
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
}
#endif

  result = pthread_create(thread, &attr, entry, arg);
  if (result != 0)
    return NULL;

  pthread_setname_np(*thread, name);
  pthread_attr_destroy(&attr);
  return thread;
}

os_mutex_t *os_mutex_create(void)
{
  int result;
  pthread_mutex_t *mutex = os_malloc(sizeof(*mutex));

  pthread_mutexattr_t attr;
  CC_STATIC_ASSERT(_POSIX_THREAD_PRIO_INHERIT > 0);
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
  result = pthread_mutex_init(mutex, &attr);

  if (result != 0)
  {
    return NULL;
  }

  return mutex;
}

void os_mutex_lock(os_mutex_t *_mutex)
{
  if(_mutex != NULL)
  {
    pthread_mutex_t *mutex = _mutex;
    pthread_mutex_lock(mutex);
  }
}

void os_mutex_unlock(os_mutex_t *_mutex)
{
  if (_mutex != NULL)
  {
    pthread_mutex_t *mutex = _mutex;
    pthread_mutex_unlock(mutex);
  }
}

void os_mutex_destroy(os_mutex_t *_mutex)
{
  if (_mutex != NULL)
  {
    pthread_mutex_t *mutex = _mutex;
    pthread_mutex_destroy(mutex);
    os_free(mutex);
  }
}

os_sem_t *os_sem_create(size_t count)
{
  os_sem_t *sem;
  pthread_mutexattr_t attr;

  sem = (os_sem_t *)os_malloc(sizeof(*sem));

  pthread_cond_init(&sem->cond, NULL);
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
  pthread_mutex_init(&sem->mutex, &attr);
  sem->count = count;

  return sem;
}

int os_sem_wait(os_sem_t *sem, uint32_t time)
{
  struct timespec ts;
  int error = 0;
  uint64_t nsec = (uint64_t)time * 1000 * 1000;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  nsec += ts.tv_nsec;
  if (nsec > NSECS_PER_SEC)
  {
    ts.tv_sec += nsec / NSECS_PER_SEC;
    nsec %= NSECS_PER_SEC;
  }
  ts.tv_nsec = nsec;

  pthread_mutex_lock(&sem->mutex);
  while (sem->count == 0)
  {
    if (time != OS_WAIT_FOREVER)
    {
      error = pthread_cond_timedwait(&sem->cond, &sem->mutex, &ts);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
      if (error)
      {
        goto timeout;
      }
    }
    else
    {
      error = pthread_cond_wait(&sem->cond, &sem->mutex);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
    }
  }

  sem->count--;

timeout:
  pthread_mutex_unlock(&sem->mutex);
  return (error) ? 1 : 0;
}

void os_sem_signal(os_sem_t *sem)
{
  pthread_mutex_lock(&sem->mutex);
  sem->count++;
  pthread_mutex_unlock(&sem->mutex);
  pthread_cond_signal(&sem->cond);
}

void os_sem_destroy(os_sem_t *sem)
{
  pthread_cond_destroy(&sem->cond);
  pthread_mutex_destroy(&sem->mutex);
  os_free(sem);
}

void os_usleep(uint32_t usec)
{
  struct timespec ts;
  struct timespec remain;

  ts.tv_sec = usec / USECS_PER_SEC;
  ts.tv_nsec = (usec % USECS_PER_SEC) * 1000;
  while (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &remain) == -1)
  {
    ts = remain;
  }
}

uint64_t os_get_current_time_us(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)(ts.tv_sec) * 1000ll * 1000ll + (uint64_t)(ts.tv_nsec / 1000);
}

void os_get_current_timestamp(pf_log_book_ts_t *p_ts)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  p_ts->status = PF_TS_STATUS_GLOBAL;
  p_ts->sec_hi = (uint16_t)((uint64_t)(ts.tv_sec) >> 32U);
  p_ts->sec_lo = ts.tv_sec & 0xFFFFFFFF;
  p_ts->nano_sec = ts.tv_nsec;
}


os_event_t *os_event_create(void)
{
  os_event_t *event;
  pthread_mutexattr_t attr;

  event = (os_event_t *)os_malloc(sizeof(*event));

  pthread_cond_init(&event->cond, NULL);
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
  pthread_mutex_init(&event->mutex, &attr);
  event->flags = 0;

  return event;
}

int os_event_wait(os_event_t *event, uint32_t mask, uint32_t *value, uint32_t time_in_us)
{
  struct timespec ts;
  int error = 0;
  uint64_t nsec = (uint64_t)time_in_us * 1000;

  if (time_in_us != OS_WAIT_FOREVER)
  {
    clock_gettime(CLOCK_MONOTONIC, &ts);
    nsec += ts.tv_nsec;

    ts.tv_sec += nsec / NSECS_PER_SEC;
    ts.tv_nsec = nsec % NSECS_PER_SEC;
  }

  pthread_mutex_lock(&event->mutex);

  while ((event->flags & mask) == 0)
  {
    if (time_in_us != OS_WAIT_FOREVER)
    {
      error = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
      if (error)
      {
        goto timeout;
      }
    }
    else
    {
      error = pthread_cond_wait(&event->cond, &event->mutex);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
    }
  }

timeout:
  *value = event->flags & mask;
  pthread_mutex_unlock(&event->mutex);
  return (error) ? 1 : 0;
}

void os_event_set(os_event_t *event, uint32_t value)
{
  pthread_mutex_lock(&event->mutex);
  event->flags |= value;
  pthread_mutex_unlock(&event->mutex);
  pthread_cond_signal(&event->cond);
}

void os_event_clr(os_event_t *event, uint32_t value)
{
  pthread_mutex_lock(&event->mutex);
  event->flags &= ~value;
  pthread_mutex_unlock(&event->mutex);
  pthread_cond_signal(&event->cond);
}

void os_event_destroy(os_event_t *event)
{
  pthread_cond_destroy(&event->cond);
  pthread_mutex_destroy(&event->mutex);
  os_free(event);
}

os_mbox_t *os_mbox_create(size_t size)
{
  os_mbox_t *mbox;
  pthread_mutexattr_t attr;

  mbox = (os_mbox_t *)os_malloc(sizeof(*mbox) + size * sizeof(void *));

  pthread_cond_init(&mbox->cond, NULL);
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
  pthread_mutex_init(&mbox->mutex, &attr);

  mbox->r = 0;
  mbox->w = 0;
  mbox->count = 0;
  mbox->size = size;

  return mbox;
}

int os_mbox_fetch(os_mbox_t *mbox, void **msg, uint32_t time)
{
  if (mbox == NULL)
  {
    return 1; // error
  }

  struct timespec ts;
  int error = 0;
  uint64_t nsec = (uint64_t)time * 1000 * 1000;

  if (time != OS_WAIT_FOREVER)
  {
    clock_gettime(CLOCK_MONOTONIC, &ts);
    nsec += ts.tv_nsec;

    ts.tv_sec += nsec / NSECS_PER_SEC;
    ts.tv_nsec = nsec % NSECS_PER_SEC;
  }

  pthread_mutex_lock(&mbox->mutex);

  while (mbox->count == 0)
  {
    if (time != OS_WAIT_FOREVER)
    {
      error = pthread_cond_timedwait(&mbox->cond, &mbox->mutex, &ts);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
      if (error)
      {
        goto timeout;
      }
    }
    else
    {
      error = pthread_cond_wait(&mbox->cond, &mbox->mutex);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
    }
  }

  *msg = mbox->msg[mbox->r++];
  if (mbox->r == mbox->size)
    mbox->r = 0;

  mbox->count--;

timeout:
  pthread_mutex_unlock(&mbox->mutex);
  pthread_cond_signal(&mbox->cond);

  return (error) ? 1 : 0;
}

int os_mbox_post(os_mbox_t *mbox, void *msg, uint32_t time)
{
  if (mbox == NULL)
  {
    return 1; // error
  }

  struct timespec ts;
  int error = 0;
  uint64_t nsec = (uint64_t)time * 1000 * 1000;

  if (time != OS_WAIT_FOREVER)
  {
    clock_gettime(CLOCK_MONOTONIC, &ts);
    nsec += ts.tv_nsec;

    ts.tv_sec += nsec / NSECS_PER_SEC;
    ts.tv_nsec = nsec % NSECS_PER_SEC;
  }

  pthread_mutex_lock(&mbox->mutex);

  while (mbox->count == mbox->size)
  {
    if (time != OS_WAIT_FOREVER)
    {
      error = pthread_cond_timedwait(&mbox->cond, &mbox->mutex, &ts);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
      if (error)
      {
        goto timeout;
      }
    }
    else
    {
      error = pthread_cond_wait(&mbox->cond, &mbox->mutex);
#ifdef _DEBUG
      assert(error != EINVAL);
#endif // _DEBUG
    }
  }

  mbox->msg[mbox->w++] = msg;
  if (mbox->w == mbox->size)
    mbox->w = 0;

  mbox->count++;

timeout:
  pthread_mutex_unlock(&mbox->mutex);
  pthread_cond_signal(&mbox->cond);

  return (error) ? 1 : 0;
}

void os_mbox_destroy(os_mbox_t *mbox)
{
  if(mbox != NULL)
  {
    pthread_cond_destroy(&mbox->cond);
    pthread_mutex_destroy(&mbox->mutex);
    os_free(mbox);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static void *os_timer_thread(void *arg)
{
  os_timer_t *timer = arg;
  sigset_t sigset;
  siginfo_t si;
  struct timespec tmo;

  rpmalloc_thread_initialize();

  timer->thread_id = (pid_t)syscall(SYS_gettid);

  /* Add SIGALRM */
  sigemptyset(&sigset);
  sigprocmask(SIG_BLOCK, &sigset, NULL);
  sigaddset(&sigset, SIGALRM);

  tmo.tv_sec = 0;
  tmo.tv_nsec = 500 * 1000 * 1000;

  while (!timer->exit)
  {
    int sig = sigtimedwait(&sigset, &si, &tmo);
    if (sig == SIGALRM)
    {
      if (timer->fn)
      {
        timer->fn(timer, timer->arg);
      }
    }
  }

  printf("[INFO ] os_timer_thread terminated\n");
  rpmalloc_thread_finalize();
  return NULL;
}

os_timer_t *os_timer_create(uint32_t us, void (*fn) (os_timer_t *, void *arg),
                            void *arg, bool oneshot)
{
  os_timer_t *timer;
  struct sigevent sev;
  sigset_t sigset;

  /* Block SIGALRM in calling thread */
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  timer = (os_timer_t *)os_malloc(sizeof(*timer));

  timer->exit = false;
  timer->thread_id = 0;
  timer->fn = fn;
  timer->arg = arg;
  timer->us = us;
  timer->oneshot = oneshot;

  /* Create timer thread */
  timer->thread = os_thread_create("os_timer", TIMER_PRIO, 1024, os_timer_thread, timer);
  if (timer->thread == NULL)
  {
    os_free(timer);
    return NULL;
  }

  /* Wait until timer thread sets its (kernel) thread id */
  do
  {
    sched_yield();
  } while (timer->thread_id == 0);

  /* Create timer */
  sev.sigev_notify = SIGEV_THREAD_ID;
  sev.sigev_value.sival_ptr = timer;
  sev._sigev_un._tid = timer->thread_id;
  sev.sigev_signo = SIGALRM;
  sev.sigev_notify_attributes = NULL;

  if (timer_create(CLOCK_MONOTONIC, &sev, &timer->timerid) == -1)
  {
    os_free(timer);
    return NULL;
  }

  return timer;
}

void os_timer_set(os_timer_t *timer, uint32_t us)
{
  timer->us = us;
}

void os_timer_start(os_timer_t *timer)
{
  if(timer != NULL)
  {
    struct itimerspec its;

    /* Start timer */
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1000 * timer->us;
    its.it_interval.tv_sec = (timer->oneshot) ? 0 : its.it_value.tv_sec;
    its.it_interval.tv_nsec = (timer->oneshot) ? 0 : its.it_value.tv_nsec;
    timer_settime(timer->timerid, 0, &its, NULL);
  }
}

void os_timer_stop(os_timer_t *timer)
{
  if(timer != NULL)
  {
    struct itimerspec its;

    /* Stop timer */
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    timer_settime(timer->timerid, 0, &its, NULL);
  }
}

void os_timer_destroy(os_timer_t *timer)
{
  if(timer != NULL)
  {
    timer->exit = true;
    pthread_join(*timer->thread, NULL);
    timer_delete(timer->timerid);
    os_free(timer);
  }
}

//////////////////////////////////////////////////////////////////////////

os_buf_t *os_buf_alloc(uint32_t length)
{
  os_buf_t *p = NULL;

  // first check the static pbufs
  os_mutex_lock(pbuf_mutex);
  for (size_t i = 0; i < MAX_STATIC_BUFS; i++)
  {
    if (bufs[i].in_use == false)
    {
      bufs[i].in_use = true;
      p = (os_buf_t *)(bufs[i].data);
      break;
    }
  }
  os_mutex_unlock(pbuf_mutex);

  if(p == NULL)
  {
    p = os_malloc(sizeof(os_buf_t) + length);
  }

  p->payload = (void *)((uint8_t *)p + sizeof(os_buf_t));  /* Payload follows header struct */
  p->len = length;

  return p;
}

void os_buf_free(os_buf_t *p)
{
  // free of NULL pointer is allowed, so just return here
  if (p == NULL)
  {
    return;
  }

  // first check the static pbufs
  uint8_t *ptr_to_check = (uint8_t *)p;
  os_mutex_lock(pbuf_mutex);
  for (size_t i = 0; i < MAX_STATIC_BUFS; i++)
  {
    if ((bufs[i].in_use == true) && (ptr_to_check == bufs[i].data))
    {
      bufs[i].in_use = false;
      os_mutex_unlock(pbuf_mutex);
      return;
    }
  }
  os_mutex_unlock(pbuf_mutex);
  os_free(p);
}

uint8_t os_buf_header(os_buf_t *p, int16_t header_size_increment)
{
  (void)p;
  (void)header_size_increment;
  return 255;
}

////////////////////////////////////////////////////////////////////////////////////////////////

int os_set_ip_suite(
  void       *arg,
  os_ipaddr_t ipaddr,
  os_ipaddr_t netmask,
  os_ipaddr_t gw,
  bool        b_temporary)
{
  int rv                = 0;
  app_data_t *p_appdata = (app_data_t *)arg;

  const uint32_t def_ip = __builtin_bswap32(p_appdata->def_ip);

  if (ipaddr == 0)
  {
    set_ip_address_to_interface(p_appdata, def_ip);
  }
  else
  {
    set_ip_address_to_interface(p_appdata, ipaddr);
  }

  pf_full_ip_suite_t data;
  memset(&data, 0, sizeof(data));
  if(b_temporary == false)
  {
    data.ip_suite.ip_addr    = __builtin_bswap32(ipaddr);
    data.ip_suite.ip_mask    = __builtin_bswap32(netmask);
    data.ip_suite.ip_gateway = __builtin_bswap32(gw);
  }

  // first read the file and compare the contents
  bool b_data_different = true;
  FILE *fp = fopen(IP_SETTINGS_DATA_FILE_NAME, "r");
  if (fp != NULL)
  {
    pf_full_ip_suite_t temp = { 0 };
    fread(&temp, sizeof(temp), 1, fp);
    fclose(fp);
    if (memcmp(&temp, &data, sizeof(data)) == 0)
    {
      b_data_different = false;
    }
  }

  if(b_data_different)
  {
    fp = fopen(IP_SETTINGS_DATA_FILE_NAME, "w");
    if (fp != NULL)
    {
      fwrite(&data, sizeof(data), 1, fp);
      fclose(fp);
      system("sync");
    }
    else
    {
      rv = -1;
    }
  }

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "os_set_ip_suite: set new IP parameters %s:\n",
           b_temporary ? "temporarily" : "PERMANENT");
    print_ip_address("IP address: ", __builtin_bswap32(ipaddr));
    print_ip_address("Netmask:    ", __builtin_bswap32(netmask));
    print_ip_address("Gateway:    ", __builtin_bswap32(gw));
    if (b_temporary)
    {
      printf("(note: but store factory IP defaults to disk)\n");
    }
    printf("\n");
  }
  return rv;
}

int os_set_station_name(void *arg, const char *name, bool b_temporary)
{
  int rv = 0;
  app_data_t *p_appdata = (app_data_t *)arg;
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "set new station name %s:\n%s\n\n",
           b_temporary ? "temporarily" : "PERMANENT",
           name[0] == '\0' ? "<empty>" : name);
  }

  char name_of_station[STATTION_NAME_SIZE];
  memset(name_of_station, 0, sizeof(name_of_station));
  if (b_temporary == false)
  {
    strcpy(name_of_station, name);
  }

  // first read the file and compare the contents
  bool b_data_different = true;
  FILE *fp = fopen(NAME_OF_STATION_DATA_FILE_NAME, "r");
  if (fp != NULL)
  {
    char temp[STATTION_NAME_SIZE] = { 0 };
    fread(temp, sizeof(temp), 1, fp);
    fclose(fp);
    if (memcmp(name_of_station, temp, sizeof(name_of_station)) == 0)
    {
      b_data_different = false;
    }
  }

  if(b_data_different)
  {
    fp = fopen(NAME_OF_STATION_DATA_FILE_NAME, "w");
    if (fp != NULL)
    {
      fwrite(&name_of_station, sizeof(name_of_station), 1, fp);
      fclose(fp);
      system("sync");
    }
    else
    {
      rv = -1;
    }
  }
  return rv;
}

int os_save_im_data(pnet_t *net)
{  
  bool b_data_different = true;
  int rv = 0;
  // first read the file and compare the contents
  FILE *fp = fopen(IM_DATA_FILE_NAME, "r");
  if (fp != NULL)
  {
    pnet_im_1_t im_1_data = { 0 };
    pnet_im_2_t im_2_data = { 0 };
    pnet_im_3_t im_3_data = { 0 };
    pf_check_peers_t check_peers = { 0 };
    uint32_t adjust_peer_boundary = 0;

    fread(&im_1_data, sizeof(im_1_data), 1, fp);
    fread(&im_2_data, sizeof(im_2_data), 1, fp);
    fread(&im_3_data, sizeof(im_3_data), 1, fp);
    fread(&check_peers, sizeof(check_peers), 1, fp);
    fread(&adjust_peer_boundary, sizeof(adjust_peer_boundary), 1, fp);
    fclose(fp);

    b_data_different = false;
    if (memcmp(&im_1_data, &(net->fspm_cfg.im_1_data), sizeof(im_1_data)) != 0)
    {
      b_data_different = true;
    }
    if (memcmp(&im_2_data, &(net->fspm_cfg.im_2_data), sizeof(im_2_data)) != 0)
    {
      b_data_different = true;
    }
    if (memcmp(&im_3_data, &(net->fspm_cfg.im_3_data), sizeof(im_3_data)) != 0)
    {
      b_data_different = true;
    }
    if (memcmp(&check_peers, &(net->temp_check_peers_data), sizeof(check_peers)) != 0)
    {
      b_data_different = true;
    }
    if (adjust_peer_boundary != net->adjust_peer_to_peer_boundary)
    {
      b_data_different = true;
    }
  }
  
  // save to disk only if data are different -> protect the SD card
  if(b_data_different)
  {
    fp = fopen(IM_DATA_FILE_NAME, "w");
    if (fp != NULL)
    {
      fwrite(&(net->fspm_cfg.im_1_data), sizeof(net->fspm_cfg.im_1_data), 1, fp);
      fwrite(&(net->fspm_cfg.im_2_data), sizeof(net->fspm_cfg.im_2_data), 1, fp);
      fwrite(&(net->fspm_cfg.im_3_data), sizeof(net->fspm_cfg.im_3_data), 1, fp);
      fwrite(&(net->temp_check_peers_data), sizeof(net->temp_check_peers_data), 1, fp);
      fwrite(&(net->adjust_peer_to_peer_boundary), sizeof(net->adjust_peer_to_peer_boundary), 1, fp);
      fclose(fp);
      system("sync");
    }
    else
    {
      rv = -1;
    }
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////////

int os_get_ip_suite(
  os_ipaddr_t *p_ipaddr,
  os_ipaddr_t *p_netmask,
  os_ipaddr_t *p_gw,
  char *p_device_name,
  pnet_cfg_t *p_device_cfg)
{
  int rv = 0;
  pf_full_ip_suite_t data;
  memset(&data, 0, sizeof(data));

  FILE *fp = fopen(IP_SETTINGS_DATA_FILE_NAME, "r");
  if (fp != NULL)
  {
    fread(&data, sizeof(data), 1, fp);
    fclose(fp);
    *p_ipaddr = data.ip_suite.ip_addr;
    *p_netmask = data.ip_suite.ip_mask;
    *p_gw = data.ip_suite.ip_gateway;
  }
  else
  {
    *p_ipaddr  = 0;
    *p_netmask = 0;
    *p_gw      = 0;
    rv         = -1;
  }

  char name_of_station[STATTION_NAME_SIZE];
  memset(name_of_station, 0, sizeof(name_of_station));

  fp = fopen(NAME_OF_STATION_DATA_FILE_NAME, "r");
  if(fp != NULL)
  {
    fread(name_of_station, sizeof(name_of_station), 1, fp);
    fclose(fp);
    strcpy(p_device_name, name_of_station);
  }
  else
  {
    p_device_name[0] = '\0';
    rv               = -1;
  }

  fp = fopen(IM_DATA_FILE_NAME, "r");
  if (fp != NULL)
  {
    fread(&(p_device_cfg->im_1_data), sizeof(p_device_cfg->im_1_data), 1, fp);
    fread(&(p_device_cfg->im_2_data), sizeof(p_device_cfg->im_2_data), 1, fp);
    fread(&(p_device_cfg->im_3_data), sizeof(p_device_cfg->im_3_data), 1, fp);
    fread(&(p_device_cfg->temp_check_peers_data), sizeof(p_device_cfg->temp_check_peers_data), 1, fp);
    fread(&(p_device_cfg->adjust_peer_to_peer_boundary), sizeof(p_device_cfg->adjust_peer_to_peer_boundary), 1, fp);
    fclose(fp);
  }

  return rv;
}

void os_get_button(void *arg, uint16_t id, bool *p_pressed)
{
  (void)(id);
  (void)(p_pressed);
}

void os_set_led(void *arg, uint16_t id, bool on)
{
  app_data_t *p_appdata = (app_data_t *)arg;
  if(p_appdata->arguments.use_led != 0)
  {
    set_led(p_appdata->i2c_file, id, on);
  }
  else if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Set LED %u to %i\n", id, on);
  }
}

//////////////////////////////////////////////////////////////////////////

void log_sys_cmd(app_data_t *p_appdata, const char *cmd)
{
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "**** Executing command: %s ****\n", cmd);
  }
}

void set_ip_address_to_interface(app_data_t *p_appdata, uint32_t ipaddr)
{
  if (ipaddr != 0)
  {
    char cmd[STATTION_NAME_SIZE * 2];
    char strIpAddr[64];
    sprint_ip_address(strIpAddr, sizeof(strIpAddr), __builtin_bswap32(ipaddr));
    sprintf(cmd, "ifconfig %s %s", p_appdata->arguments.eth_interface, strIpAddr);
    log_sys_cmd(p_appdata, cmd);
    system(cmd);
  }
}
