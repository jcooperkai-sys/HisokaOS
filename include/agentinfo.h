/* agentinfo.h - the "agentic" self-description of HisokaOS. Emits a machine-readable
 * JSON descriptor (and a markdown brief) so that an AI agent inspecting the system
 * can learn what HisokaOS is, what it can do, and how to drive it. */
#ifndef HISOKA_AGENTINFO_H
#define HISOKA_AGENTINFO_H

void agentinfo_describe(int save);   /* print the descriptor; if save, also write /System/agent.json */
void agentinfo_seed(void);           /* drop /System/AGENTS.md + /etc/llms.txt at boot */
int  agentinfo_enabled(void);        /* 1 if the config allows publishing the descriptor */

#endif
