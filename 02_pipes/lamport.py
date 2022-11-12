import logging
import time
import random
from multiprocessing import Process, Pipe
from multiprocessing.managers import BaseManager


NUM_PIPES = 6
NUM_WORKERS = 3


class Entry:
    """ Database document """

    def __init__(self, pi, clock, ko_count):
        self.pi = pi
        self.clock = clock
        self.ko_count = ko_count

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return f"(PI: {self.pi}; CLOCK: {self.clock}; KO_COUNT: {self.ko_count})"


class Database:
    """ Database demo """

    def __init__(self):
        self.storage = []

    def add(self, item: Entry):
        """ Add new record to database """
        self.storage.append(item)

    def update(self, pi, clock, ko_count):
        """ Update existing record in database """
        idx = pi - 1
        if idx >= len(self.storage):
            return
        self.storage[idx].clock = clock
        self.storage[idx].ko_count = ko_count

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return f"{str(self.storage)}"


class Message:
    """ Pipe message """
    REQ = 1
    ACK = 2
    END = 3

    def __init__(self, msg_type, sender_pi, sender_clock, recipient_pi=None):
        self.msg_type = msg_type
        self.sender_pi = sender_pi
        self.sender_clock = sender_clock
        self.recipient_pi = recipient_pi

    def is_req(self):
        """ Check if message is of type request """
        return self.msg_type == Message.REQ

    def is_ack(self, pi=None):
        """ Check if message is of type acknowledge """
        return self.msg_type == Message.ACK and self.recipient_pi == pi

    def is_end(self):
        """ Check if message is of type end """
        return self.msg_type == Message.END

    def _get_msg_type(self):
        if self.msg_type == Message.REQ:
            return "REQ"
        if self.msg_type == Message.ACK:
            return "ACK"
        if self.msg_type == Message.END:
            return "END"
        return None

    def __eq__(self, other):
        if isinstance(other, Message):
            return self.msg_type == other.msg_type \
                and self.sender_pi == other.sender_pi \
                and self.sender_clock == other.sender_clock
        return False

    def __lt__(self, other):
        if self.sender_clock < other.sender_clock:
            return True
        if self.sender_clock > other.sender_clock:
            return False
        if self.sender_clock == other.sender_clock:
            return self.sender_pi < other.sender_pi
        return False

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return f"{self._get_msg_type()}({self.sender_pi}, {self.sender_clock})"


class RequestQueue:
    """ Message requests queue """

    def __init__(self):
        self.queue = []

    def add(self, req: Message):
        """ Add new message request to queue """
        if req.is_req():
            self.queue.append(req)
            self.queue.sort()

    def size(self):
        """ Get message queue size """
        return len(self.queue)

    def remove(self, req: Message):
        """ Remove message from message queue """
        # override msg type for __eq__ method
        req.msg_type = Message.REQ
        self.queue.remove(req)

    def get_first(self):
        """ Get the first message from the message queue """
        if self.size() > 0:
            return self.queue[0]
        return None

    def is_pi_request_first(self, pi):
        """ Check if the first message in the queue is sent by process pi """
        first = self.get_first()
        if first is None:
            return False
        return first.sender_pi == pi

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return str(self.queue)


class CustomManager(BaseManager):
    """ Shared memory manger """
    pass


def database_job(db: Database, pi: int, clock: int, ko_count: int):
    """ K.O. job """
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    logging.debug(f"PI {pi} started a database job")
    db.update(pi, clock, ko_count)
    logging.info(f"PI {pi} updated database: {str(db)}")
    r = random.randint(100, 2000) / 1000
    time.sleep(r)
    logging.debug(f"PI {pi} finished database job")


def push_messages(pi: int, pipes: list, msg: Message):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    logger.info(f"PI {pi} sent a message {msg}")
    for pipe in pipes:
        pipe.send(msg)


def get_messages(pi: int, pipes: list) -> Message:
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    for pipe in pipes:
        if pipe.poll():
            msg = pipe.recv()
            if msg.recipient_pi is None \
                    or msg.recipient_pi == pi:
                logging.info(f"PI {pi} recieved message {msg}")
                yield msg


def worker(db, pi, pipes_read, pipes_write):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    logging.info(f"Starting process {pi}")
    time.sleep(1)

    requests_queue = RequestQueue()
    clock = 0
    ko_count = 0
    ko_count_break_point = 2  # TODO set to 5
    ack_required = len(pipes_write)
    ack_received = 0

    while True:
        # if not done send a new request
        if ko_count < ko_count_break_point:
            # craft request message
            msg = Message(Message.REQ, pi, clock)
            # add my request to queue
            requests_queue.add(msg)
            logger.debug(
                f"PI {pi} updated message queue: {requests_queue} clock: {clock}")
            # broadcast request
            push_messages(pi, pipes_write, msg)

        # just for sanity
        time.sleep(1)

        # reading and waiting for responses
        while True:
            # process all incoming messages from all pipes
            for msg in get_messages(pi, pipes_read):

                # a new request from somebody
                if msg.is_req():
                    # update clock
                    clock = max(clock, msg.sender_clock) + 1
                    # save request to queueu
                    requests_queue.add(msg)
                    logger.debug(
                        f"PI {pi} updated message queue: {requests_queue} clock: {clock}")
                    # send response ack
                    msg = Message(Message.ACK, pi, clock, msg.sender_pi)
                    push_messages(pi, pipes_write, msg)

                # recevied ack for my request
                if msg.is_ack(pi):
                    ack_received += 1
                    # update clock
                    clock = max(clock, msg.sender_clock) + 1

                # received end message for someones exit from K.O.
                if msg.is_end():
                    # update clock
                    clock = max(clock, msg.sender_clock) + 1
                    # remove original request from queue
                    requests_queue.remove(msg)
                    logger.debug(
                        f"PI {pi} updated message queue: {requests_queue} clock: {clock}")

            # check if process can go to K.O. now
            # check if process got all responses
            # check if process request is the first in queue
            if ack_received == ack_required \
                    and requests_queue.is_pi_request_first(pi):
                break

        if ko_count < ko_count_break_point:
            logger.debug(
                f"PI {pi} received all ACKs and is entering K.O.: clock = {clock} queue = {requests_queue}")

            # remove request from queue
            my_req = requests_queue.get_first()
            assert my_req is not None
            assert my_req.sender_pi == pi
            requests_queue.remove(my_req)
            logger.debug(
                f"PI {pi} updated message queue: {requests_queue} clock: {clock}")
            ack_received = 0

            # do database job
            ko_count += 1
            database_job(db, pi, clock, ko_count)

            # notify others job is done
            # send clock from original request not the current clock
            msg = Message(Message.END, pi, my_req.sender_clock)
            push_messages(pi, pipes_write, msg)


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


def main():
    """ Main process """
    logging.info('Starting main')

    # create shared database in memory
    CustomManager.register('Database', Database)

    with CustomManager() as manager:
        db = manager.Database()

        # init database
        for i in range(NUM_WORKERS):
            pi = i + 1
            entry = Entry(pi, 0, 0)
            db.add(entry)
        logging.debug(f"database: {str(db)}")

        # init pipes
        # TODO formula
        pipes = []
        for _ in range(NUM_PIPES):
            r, w = Pipe()
            pipes.append((r, w))

        # init processes
        procs = []
        for i in range(NUM_WORKERS):
            pi = i + 1
            pipes_read = get_pipes_read(pipes, pi)
            pipes_write = get_pipes_write(pipes, pi)
            proc = Process(target=worker, args=(
                db, pi, pipes_read, pipes_write, ))
            procs.append(proc)
            proc.start()

        for i in range(NUM_WORKERS):
            procs[i].join()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
