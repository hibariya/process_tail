inline int PID = <%= pid %>;
inline int FD  = <%= fd %>;

dtrace:::BEGIN
{
  processes[pid] = 0;
  self->child    = 0;
}

syscall:::entry
/processes[ppid] == -1 && self->child == 0/
{
  self->child = 1;
}

syscall:::entry
/processes[ppid] > 0 && self->child == 0/
{
  this->vforking_tid = processes[ppid];
  self->child        = (this->vforking_tid == tid) ? 0 : 1;
}

syscall:::entry
/pid == PID || self->child/
{
  self->tracked = 1;
  self->arg0 = arg0;
  self->arg1 = arg1;
  self->arg2 = arg2;
}

syscall::fork:entry
/self->tracked/
{
 processes[pid] = -1;
}

syscall::vfork:entry
/self->tracked/
{
 processes[pid] = tid;
}

syscall::exit:entry
{
 self->child    = 0;
 processes[pid] = 0;
}

syscall::exit:entry
/pid == PID/
{
  exit(0);
}

syscall:::entry
/!((probefunc == "write" || probefunc == "write_nocancel") && (FD == 0 || self->arg0 == FD))/
{
  self->tracked = 0;
  self->arg0 = 0;
  self->arg1 = 0;
  self->arg2 = 0;
}

syscall::write:return,
syscall::write_nocancel:return
/self->tracked/
{
  trace(pid);                                /* pid     */
  trace(self->arg0);                         /* fd      */
  trace(stringof(copyin(self->arg1, arg0))); /* content */

  self->tracked = 0;
  self->arg0 = 0;
  self->arg1 = 0;
  self->arg2 = 0;
}
