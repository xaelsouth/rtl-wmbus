import sys
import os
import traceback
import subprocess
import psutil
import signal
from time import time


__rtl_sdr = None
__rtl_wmbus = None


def __signal_handler(signal, frame):
    global __rtl_sdr, __rtl_wmbus

    if __rtl_wmbus:
        #print("Terminating rtl_wmbus")
        __rtl_wmbus.kill()
        __rtl_wmbus.wait()

    if __rtl_sdr:
        #print("Terminating rtl_sdr")
        __rtl_sdr.kill()
        __rtl_sdr.wait()


def _main(args):
    global __rtl_sdr, __rtl_wmbus
    
    #"/C/Program Files/PothosSDR/bin/rtl_sdr" -f 868.95M -s 1600000 - 2>/dev/null | build/rtl_wmbus_x64.exe -v
    #"C:\Program Files\PothosSDR\bin\rtl_sdr" -f 868.95M -s 1600000 - 2>NUL | build\rtl_wmbus_x64.exe -v

    try:
        signal.signal(signal.SIGINT, __signal_handler)

        __rtl_sdr = subprocess.Popen([r"c:\Program Files\PothosSDR\bin\rtl_sdr", "-f", "868.95M", "-s", "1600000", "-"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        psutil.Process(__rtl_sdr.pid).nice(psutil.REALTIME_PRIORITY_CLASS)

        __rtl_wmbus = subprocess.Popen([r"d:\rtl-wmbus\build\rtl_wmbus_x64.exe", "-v"], stdin=__rtl_sdr.stdout, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        psutil.Process(__rtl_wmbus.pid).nice(psutil.REALTIME_PRIORITY_CLASS)

        psutil.Process(os.getpid()).nice(psutil.REALTIME_PRIORITY_CLASS)

        #__rtl_sdr.stdout.close()  # Allow rtl_sdr to receive a SIGPIPE if rtl_wmbus exits.

        t1 = time()
        rate = 0
        while __rtl_sdr.poll() is None and __rtl_wmbus.poll() is None:
            s = __rtl_wmbus.stdout.readline().decode('utf-8').rstrip('\n').rstrip('\r')
            t2 = time()

            if s.find("T1;1") >= 0 or s.find("C1;1") >= 0 or s.find("S1;1") >= 0:
                rate += 1
                print(s)
            else:
                pass

            dt = t2 - t1
            if dt >= 10:
                #print(rate/dt, file=sys.stderr)
                t1 = t2
                rate = 0

        return 0

    except Exception as e:
        print(traceback.print_exc())
        return 1


if __name__ == '__main__':
    sys.exit(_main(sys.argv))
