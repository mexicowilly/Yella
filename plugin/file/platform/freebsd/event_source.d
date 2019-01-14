#pragma D option quiet
#pragma D option switchrate=10hz

syscall::write:entry
/execname != "dtrace"/
{
    self->fd = arg0
}

syscall::write:return
/execname != "dtrace" && arg0 > 0/
{
    printf("write:%d,%d\n", pid, self->fd)
}
