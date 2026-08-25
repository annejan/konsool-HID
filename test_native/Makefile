CC = gcc
CFLAGS = -Wall -Wextra -g -I. -I../main -I$(HID_HOST)/include

# Reading the report descriptor is the hid-host component's job now. Point HID_HOST at a checkout
# of your own to test this project against unreleased changes to it.
HID_HOST_VERSION ?= v0.3.0
HID_HOST ?= .deps/hid-host
HID_HOST_URL ?= https://github.com/badgeteam/esp32-component-hid-host

OBJS = test_hid.o badge_hid_drivers.o hid_layout.o hid_gamepad.o

test: $(OBJS)
	$(CC) -o $@ $(OBJS)

$(HID_HOST):
	mkdir -p $(dir $@)
	git clone -q --depth 1 --branch $(HID_HOST_VERSION) $(HID_HOST_URL) $@

# Whatever reads the component waits for it to be fetched
$(HID_HOST)/src/%.c: | $(HID_HOST) ;
$(HID_HOST)/include/%.h: | $(HID_HOST) ;

test_hid.o: test_hid.c test_descriptors.h test_reports.h ../main/badge_hid_drivers.h \
            $(HID_HOST)/include/hid_layout.h $(HID_HOST)/include/hid_gamepad.h

badge_hid_drivers.o: ../main/badge_hid_drivers.c ../main/badge_hid_drivers.h \
                     $(HID_HOST)/include/hid_layout.h $(HID_HOST)/include/hid_gamepad.h
	$(CC) $(CFLAGS) -c -o $@ $<

hid_layout.o: $(HID_HOST)/src/hid_layout.c $(HID_HOST)/include/hid_layout.h
	$(CC) $(CFLAGS) -c -o $@ $<

hid_gamepad.o: $(HID_HOST)/src/hid_gamepad.c $(HID_HOST)/include/hid_gamepad.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f *.o test

# Also throws away the fetched component
.PHONY: distclean
distclean: clean
	rm -rf .deps
