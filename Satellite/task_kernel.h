#ifndef _TASK_KERNEL_
#define _TASK_KERNEL_

int
authentifyDialogue(struct task *task, struct dialogueDemande *dialogueDemande);

void
dialogueAnticipant(struct task *task);

void
dialogueRegular(struct task *task);

#endif
