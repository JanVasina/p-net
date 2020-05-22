#ifndef OPTIONS_H
#define OPTIONS_H

#ifndef LOG_LEVEL
# ifdef _DEBUG
#  define LOG_LEVEL         (LOG_LEVEL_INFO)
# else
#  define LOG_LEVEL         (LOG_LEVEL_WARNING)
# endif
#endif

#ifndef PF_ETH_LOG
#define PF_ETH_LOG      		(LOG_STATE_ON)
#endif

#ifndef PF_CPM_LOG
#define PF_CPM_LOG      		(LOG_STATE_ON)
#endif

#ifndef PF_PPM_LOG
#define PF_PPM_LOG      		(LOG_STATE_ON)
#endif

#ifndef PF_DCP_LOG
#define PF_DCP_LOG      		(LOG_STATE_ON)
#endif

#ifndef PF_RPC_LOG
#define PF_RPC_LOG      		(LOG_STATE_ON)
#endif

#ifndef PF_ALARM_LOG
#define PF_ALARM_LOG      		(LOG_STATE_ON)
#endif

#ifndef PF_AL_BUF_LOG
#define PF_AL_BUF_LOG      		(LOG_STATE_ON)
#endif

#ifndef PNET_LOG
#define PNET_LOG      			(LOG_STATE_ON)
#endif

#endif  /* OPTIONS_H */
