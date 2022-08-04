TARGET = kernel.elf
OBJS = main.o graphics.o font.o hankaku.o console.o newlib_support.o logging.o asmfunc.o \
		segment.o libcxx_support.o paging.o memory_manager.o interrupt.o timer.o task.o \
		run_application.o syscall.o elf.o pci.o usb/memory.o usb/xhci/xhci.o usb/xhci/devmgr.o \
		usb/xhci/ring.o usb/xhci/port.o
CFLAGS   += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone \
            -fno-exceptions -fno-rtti -std=c++17
LDFLAGS  += --entry KernelMain -z norelro --image-base 0x100000 --static


.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o kernel.elf $(OBJS) -lc -lc++

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<
