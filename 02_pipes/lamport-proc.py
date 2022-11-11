import logging
import time
import random
from multiprocessing import Process, Pipe


NUM_PIPES = 6
NUM_WORKERS = 3


DATABASE = []


class DbEntry:
    def __init__(self, pi, clock, ko_count):
        self.pi = pi
        self.clock = clock
        self.ko_count = ko_count
    def __repr__(self):
        return self.__str__()
    def __str__(self):
        return f"[ PI: {self.pi} CLOCK: {self.clock} KO_COUNT: {self.ko_count} ]"


class Message:
    REQ = 1
    ACK = 2
    END = 3
    def __init__(self, msg_type, pi, clock):
        self.msg_type = msg_type
        self.pi = pi
        self.clock = clock
    def is_req(self):
        return self.msg_type == Message.REQ
    def is_ack(self):
        return self.msg_type == Message.ACK
    def is_end(self):
        return self.msg_type == Message.END
    def _get_msg_type(self):
        if self.msg_type == Message.REQ:
            return "REQ"
        if self.msg_type == Message.ACK:
            return "ACK"
        if self.msg_type == Message.END:
            return "END"
    def __lt__(self, other):
        if self.clock < other.clock:
            return True
        if self.clock > other.clock:
            return False
        if self.clock == other.clock:
            return self.pi < other.pi
    def __repr__(self):
        return self.__str__()
    def __str__(self):
        return f"<{self._get_msg_type()} ({self.pi}, {self.clock})>"
 #sadrži identifikator procesa, vrijednost logičkog sata procesa te broj ulazaka u kritički odsječak procesa. 


class RequestQueue:
    def __init__(self):
        self.queue = []
    def add(self, req):
        self.queue.append(req)
        self.queue.sort()
    def size(self):
        return len(self.queue)
    def remove(self, req):
        # TODO implement
        pass
    def __repr__(self):
        return self.__str__()
    def __str__(self):
        return str(self.queue)




# TODO remove
def send_request(pi, pipe):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    # TODO form actual lamport request
    logger.debug(f"pi {pi} is sending a message to pipe")
    pipe.send(f"pi {pi} hoce u KO")


def broadcast_request(pi, clock, pipes):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    msg = Message(Message.REQ, pi, clock)
    for pipe in pipes:
        logger.debug(f"pi {pi} is sending a message {msg} to pipe")
        pipe.send(msg)


def read_message(pi, pipe) -> Message:
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    # TODO
    logger.debug(f"pi {pi} is reading a message from pipe")
    msg = pipe.recv()
    logger.info(f"pi {pi} got message from pipe: < {msg} >")
    return msg



def run_database_job(pi, clock, ko_count):
    # TODO locking shared database ???
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    logging.info(f"Pi {pi} is starting a database job")
    idx = pi - 1
    DATABASE[idx].clock = clock
    DATABASE[idx].ko_count = ko_count
    logging.info(f"pi {pi} updated database: {DATABASE}")
    r = random.randint(100, 2000) / 1000
    time.sleep(r)
    logging.info(f"Pi {pi} finished database job")

def get_messages(pi, pipes):
    # TODO not sure if this is good
    for pipe in pipes:
        if pipe.poll():
            yield read_message(pi, pipe)


def run_worker(pi, pipes_read, pipes_write):
    # TODO param send array and recv array and loop while 1
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    logging.debug(f"Starting process {pi}")

    requests_queue = RequestQueue()
    clock = 0
    ko_count = 0
    ko_count_break_point = 5

    while True:
        # send request
        if ko_count < ko_count_break_point:
            broadcast_request(pi, clock, pipes_write)

        # read response
        for msg in get_messages(pi, pipes_read):
            logger.debug(f"msg in pi {pi} => {msg}")
            if msg.is_req():
                requests_queue.add(msg)
                logger.debug(f"queue: {requests_queue}")
             
             # TODO process response
             # 1 - new req
             # 2 - ack of my req
             # 3 - someone exited ko

        # check if can go in KO
        #for pipe in pipes_read:
        #    read_message(pi, pipe)

        if random.randint(0, 3) == 0:
            break

        # read everyting


    # for _ in range(3):
    # for pipe in pipes_write:
    #     send_request(pi, pipe)
    #     time.sleep(1)

    # for pipe in pipes_read:
    #     read_message(pi, pipe)

    # send_request(pi, pipes_write[0])
    # for pipe in pipes_read:
    #     read_message(pi, pipe)

#     if pi == 1:
#         pipes[0][1].send(f"{pi} says hello")
#         pipes[5][1].send(f"{pi} says kurac")
#         logging.debug("sent succs")
#     if pi == 2:
#         msg = pipes[0][0].recv()
#         logger.info(f"{pi} received: {msg}")
#         # also send
#         pipes[2][1].send(f"{pi} says jebi si mater")
#     if pi == 3:
#         msg = pipes[5][0].recv()
#         logger.info(f"{pi} received: {msg}")
#         msg = pipes[2][0].recv()
#         logger.info(f"{pi} received opet: {msg}")


    time.sleep(2)
    return


def get_pipes_read(pipes, pi):
    # TODO smart formula
    if pi == 1:
        return [pipes[1][0], pipes[4][0]]
    if pi == 2:
        return [pipes[0][0], pipes[3][0]]
    if pi == 3:
        return [pipes[2][0], pipes[5][0]]


def get_pipes_write(pipes, pi):
    # TODO smart formula
    if pi == 1:
        return [pipes[0][1], pipes[5][1]]
    if pi == 2:
        return [pipes[1][1], pipes[2][1]]
    if pi == 3:
        return [pipes[3][1], pipes[4][1]]

def init_database():
    for i in range(NUM_WORKERS):
        pi = i + 1
        entry = DbEntry(pi, 0, 0)
        DATABASE.append(entry)
    logging.info(DATABASE)

def main():
    pipes = []
    logging.debug('Starting main')

    # init database
    init_database()

    # create pipes
    # TODO formula
    for _ in range(NUM_PIPES):
        r, w = Pipe()
        pipes.append((r, w))
    logging.debug(f"pipes: {pipes}")

    #  sadrži identifikator procesa, vrijednost logičkog sata procesa te broj ulazaka u kritički odsječak procesa. 


    # create processes
    procs = []
    for i in range(NUM_WORKERS):
        pi = i + 1
        pipes_read = get_pipes_read(pipes, pi)
        pipes_write = get_pipes_write(pipes, pi)
        proc = Process(target=run_worker, args=(pi, pipes_read, pipes_write, ))
        logging.debug(f"proc: {proc} pi: {pi} pipes_read: {pipes_read} pipes_write: {pipes_write}")
        procs.append(proc)
        proc.start()

    for i in range(NUM_WORKERS):
        procs[i].join()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
