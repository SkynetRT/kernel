include ../../Make.defs
OBJS := sem_clockwait.o sem_destroy.o sem_holder.o sem_initialize.o \
	sem_post.o sem_protect.o sem_recover.o sem_reset.o sem_rw.o \
	sem_setprotocol.o sem_tickwait.o sem_timedwait.o sem_timeout.o \
	sem_trywait.o sem_wait.o sem_waitirq.o
.PHONY: all
all: semaphore.a
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
semaphore.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)
.PHONY: clean
clean:
	$(RM) $(OBJS) semaphore.a *.d
