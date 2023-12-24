import mmap
import os

def read_from_shared_memory():
    print('\nREAD::Reading from shared memory------>\n')
    #open the shared memory object
    with open('/dev/shm/webpage', 'r') as m:
        html_content = m.read()
        print('\nREAD::Shared memory content------>\n')
        print(html_content)
        
def wait_for_signal():
    #open the shared memory object
    with open('pipe', 'r') as pf:
        signal = pf.read()
        if signal == '1':
            read_from_shared_memory()
            #reset the signal
            with open('signal_pipe', 'w') as pf:
                pf.write('0')
        else:
            print('WAITING::No signal yet')
            
if __name__ == '__main__':
    wait_for_signal()