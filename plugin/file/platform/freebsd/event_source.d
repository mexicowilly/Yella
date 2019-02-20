#pragma D option quiet
#pragma D option switchrate=10hz

syscall::write:entry, syscall::writev:entry, syscall::pwrite:entry, syscall::pwritev:entry
{
    self->fd = arg0;
}

syscall::write:return, syscall::writev:return, syscall::pwrite:return, syscall::pwritev:return
/arg0 > 0/
{
    printf("write:%d,%d\n", pid, self->fd);
}

syscall::close:entry, syscall::closefrom:entry
{
    self->fd = arg0;
}

syscall::close:return, syscall::closefrom:return
/arg0 == 0/
{
    printf("%s:%d,%d\n", probefunc, pid, self->fd);
}

syscall::exit:entry
{
    printf("exit:%d\n", pid);
}
