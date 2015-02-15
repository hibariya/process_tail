#pragma D option quiet

inline int PID = <%= pid %>;
inline int FD  = <%= fd %>;

dtrace:::BEGIN
{
  trackedpid[pid] = 0;
  self->child     = 0;
}

syscall:::entry
/trackedpid[ppid] == -1 && 0 == self->child/
{
  self->child = 1;
}

syscall:::entry
/trackedpid[ppid] > 0 && 0 == self->child/
{
  this->vforking_tid = trackedpid[ppid];
  self->child        = (this->vforking_tid == tid) ? 0 : 1;
}

syscall:::entry
/pid == PID || self->child/
{
  self->start = 1;
  self->arg0  = arg0;
  self->arg1  = arg1;
  self->arg2  = arg2;
}

syscall:::entry
/!((probefunc == "write" || probefunc == "write_nocancel") && (FD == 0 || self->arg0 == FD))/
{
  self->start = 0;
  self->arg0  = 0;
  self->arg1  = 0;
  self->arg2  = 0;
}

syscall::fork:entry
/self->start/
{
 trackedpid[pid] = -1;
}

syscall::vfork:entry
/self->start/
{
 trackedpid[pid] = tid;
}

syscall::exit:entry
{
 self->child     = 0;
 trackedpid[pid] = 0;
}

syscall::write:return,
syscall::write_nocancel:return
/self->start/
{
  trace(pid);                                /* pid     */
  trace(self->arg0);                         /* fd      */
  trace(stringof(copyin(self->arg1, arg0))); /* content */

  self->start = 0;
  self->arg0  = 0;
  self->arg1  = 0;
  self->arg2  = 0;
}
