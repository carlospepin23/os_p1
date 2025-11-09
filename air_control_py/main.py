import ctypes
import mmap
import os
import signal
import subprocess
import threading
import time

_libc = ctypes.CDLL(None, use_errno=True)

TOTAL_TAKEOFFS = 20
STRIPS = 5
shm_data = []

# TODO1: Size of shared memory for 3 integers (current process pid, radio, ground) use ctypes.sizeof()
SHM_LENGTH = ctypes.sizeof(ctypes.c_int) * 3

# Global variables and locks
planes = 0  # planes waiting
takeoffs = 0  # local takeoffs (per thread)
total_takeoffs = 0  # total takeoffs
radio_pid = 0  # Store radio PID globally for threads to access
sigterm_sent = False  # Track if SIGTERM was sent to radio
# Locks for runways and shared state
runway1_lock = threading.Lock()
runway2_lock = threading.Lock()
state_lock = threading.Lock()



def create_shared_memory():
    """Create shared memory segment for PID exchange"""
    # TODO 6:
    # 1. Encode (utf-8) the shared memory name to use with shm_open
    shm_name = b'shm_pids_'
    
    # 2. Temporarily adjust the permission mask (umask) so the memory can be created with appropriate permissions
    old_umask = os.umask(0)
    
    try:
        # 3. Use _libc.shm_open to create the shared memory
        # O_CREAT = 0o100, O_RDWR = 0o2, O_EXCL = 0o200
        fd = _libc.shm_open(shm_name, 0o102, 0o666)  # O_CREAT | O_RDWR
        if fd < 0:
            raise OSError(ctypes.get_errno(), "shm_open failed")
        
        # 4. Use _libc.ftruncate to set the size of the shared memory (SHM_LENGTH)
        if _libc.ftruncate(fd, SHM_LENGTH) != 0:
            raise OSError(ctypes.get_errno(), "ftruncate failed")
    finally:
        # 5. Restore the original permission mask (umask)
        os.umask(old_umask)
    
    # 6. Use mmap to map the shared memory
    memory = mmap.mmap(fd, SHM_LENGTH, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
    
    # 7. Create an integer-array view (use memoryview()) to access the shared memory
    data = memoryview(memory).cast('i')  # 'i' = signed int
    
    # 8. Return the file descriptor (shm_open), mmap object and memory view
    return fd, memory, data




def HandleUSR2(signum, frame):
    """Handle external signal indicating arrival of 5 new planes.
    Complete function to update waiting planes"""
    global planes
    # TODO 4: increment the global variable planes
    with state_lock:
        planes += 5

def TakeOffFunction(agent_id: int):
    """Function executed by each THREAD to control takeoffs.
    Complete using runway1_lock and runway2_lock and state_lock to synchronize"""
    global planes, takeoffs, total_takeoffs, radio_pid, sigterm_sent

    # TODO: implement the logic to control a takeoff thread
    # Use a loop that runs while total_takeoffs < TOTAL_TAKEOFFS
    # Use runway1_lock or runway2_lock to simulate runway being locked
    # Use state_lock for safe access to shared variables (planes, takeoffs, total_takeoffs)
    # Simulate the time a takeoff takes with sleep(1)
    # Send SIGUSR1 every 5 local takeoffs
    # Send SIGTERM when the total takeoffs target is reached

    while True:
        with state_lock:
            if total_takeoffs >= TOTAL_TAKEOFFS:
                if radio_pid > 0 and not sigterm_sent:
                    os.kill(radio_pid, signal.SIGTERM)
                    sigterm_sent = True
                return

            if planes <= 0:
                need_wait = True
            else:
                need_wait = False

        if need_wait:
            time.sleep(0.1)
            continue

        acquired_runway = None
        for runway in (runway1_lock, runway2_lock):
            if runway.acquire(blocking=False):
                acquired_runway = runway
                break

        if acquired_runway is None:
            time.sleep(0.05)
            continue

        try:
            with state_lock:
                if total_takeoffs >= TOTAL_TAKEOFFS:
                    return

                if planes <= 0:
                    continue

                planes -= 1
                total_takeoffs += 1
                current_total = total_takeoffs

                send_signal = (current_total % 5 == 0)

                if send_signal and radio_pid > 0:
                    os.kill(radio_pid, signal.SIGUSR1)

            # simulate time for takeoff
            time.sleep(1)

            if current_total >= TOTAL_TAKEOFFS:
                time.sleep(0.5)
                with state_lock:
                    if radio_pid > 0 and not sigterm_sent:
                        os.kill(radio_pid, signal.SIGTERM)
                        sigterm_sent = True
                return

        finally:
            acquired_runway.release()


def launch_radio():
    """unblock the SIGUSR2 signal so the child receives it"""
    def _unblock_sigusr2():
        signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGUSR2})

    # TODO 8: Launch the external 'radio' process using subprocess.Popen()

    radio_path = os.path.join(os.getcwd(), 'radio')
    
    if not os.path.isfile(radio_path):
        raise FileNotFoundError(f"Could not find radio executable at {radio_path}. CWD: {os.getcwd()}")

    process = subprocess.Popen(
        [radio_path, 'shm_pids_'],
        preexec_fn=_unblock_sigusr2
    )
    return process


def main():
    global shm_data, radio_pid
    
    # TODO 2: set the handler for the SIGUSR2 signal to HandleUSR2
    signal.signal(signal.SIGUSR2, HandleUSR2)
    # TODO 5: Create the shared memory and store the current process PID using create_shared_memory()
    fd, memory, data = create_shared_memory()
    data[0] = os.getpid()
    
    # TODO 7: Run radio and store its PID in shared memory, use the launch_radio function
    radio_process = launch_radio()
    radio_pid = radio_process.pid
    data[1] = radio_pid
    # data[2] = 0  # Ground will write its own PID
    
    # TODO 9: Create and start takeoff controller threads (STRIPS)
    threads = []
    for i in range(STRIPS): 
        t = threading.Thread(target=TakeOffFunction, args=(i,))
        t.start()
        threads.append(t)
    # TODO 10: Wait for all threads to finish their work
    for t in threads:
        t.join()

    radio_process.wait()
    
    # TODO 11: Release shared memory and close resources
    del data  # Delete memoryview before closing mmap
    memory.close()
    os.close(fd)
    _libc.shm_unlink(b'shm_pids_')


if __name__ == "__main__":
    main()