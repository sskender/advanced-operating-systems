import logging
import time
import random
from multiprocessing import Process, Pipe, Array, Manager
from multiprocessing.managers import BaseManager
import ctypes


NUM_PIPES = 6
NUM_WORKERS = 3


class Database:
    def __init__(self):
        self.storage = []
    def add(self, item):
        self.storage.append(item)
    def update(self, pi, clock, ko_count):
        idx = pi - 1
        self.storage[idx].clock = clock
        self.storage[idx].ko_count = ko_count
        print(self.storage[idx])
    def __repr__(self):
        return self.__str__()
    def __str__(self):
        return f"{str(self.storage)}"

class CustomManager(BaseManager):
    pass


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
    def __init__(self, msg_type, sender_pi, sender_clock, recipient_pi=None):
        self.msg_type = msg_type
        self.sender_pi = sender_pi
        self.sender_clock = sender_clock
        self.recipient_pi = recipient_pi
    def is_req(self):
        return self.msg_type == Message.REQ
    def is_ack(self, pi=None):
        return self.msg_type == Message.ACK and self.recipient_pi == pi
    def is_end(self):
        return self.msg_type == Message.END
    def is_for_me(self, pi):
        return self.recipient_pi == pi
    def _get_msg_type(self):
        if self.msg_type == Message.REQ:
            return "REQ"
        if self.msg_type == Message.ACK:
            return "ACK"
        if self.msg_type == Message.END:
            return "END"
    def __eq__(self, other):
        if (isinstance(other, Message)):
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
    def __repr__(self):
        return self.__str__()
    def __str__(self):
        return f"{self._get_msg_type()}({self.sender_pi}, {self.sender_clock})"



class RequestQueue:
    def __init__(self, pi):
        self.pi = pi
        self.queue = []
    def add(self, req: Message):
        if req.is_req():
            self.queue.append(req)
            self.queue.sort()
    def size(self):
        return len(self.queue)
    def remove(self, req: Message):
        req.msg_type = Message.REQ  # override for quueue
        self.queue.remove(req)
    def get_first(self):
        if self.size() > 0:
            return self.queue[0]
        return None
    def is_my_request_first(self):
        f = self.get_first()
        if f is None:
            return False
        return f.sender_pi == self.pi
        #if self.size() > 0:
        #    return self.queue[0].sender_pi == self.pi
        #return False
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


# TODO depricated
def broadcast_request(pi, clock, pipes):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    msg = Message(Message.REQ, pi, clock)
    for pipe in pipes:
        logger.debug(f"pi {pi} is sending a message to pipe: {msg}")
        pipe.send(msg)

# TODO rename this to just send???
def broadcast_message(pi, pipes, msg):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    for pipe in pipes:
        logger.debug(f"pi {pi} is sending a message to pipe: {msg}")
        pipe.send(msg)


def read_message(pi, pipe) -> Message:
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    # TODO
    #logger.debug(f"pi {pi} is reading a message from pipe")
    msg = pipe.recv()
    #logger.info(f"pi {pi} got message from pipe: < {msg} >")
    return msg



def run_database_job(db: Database, pi, clock, ko_count):
    # TODO locking shared database ???
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)

    logging.info(f"Pi {pi} is starting a database job")
    idx = pi - 1
    print(db)
    db.update(pi, clock, ko_count)
    #DATABASE[idx].clock = clock
    #DATABASE[idx].ko_count = ko_count
    logging.info(f"pi {pi} updated database: {db}")
    r = random.randint(100, 2000) / 1000
    time.sleep(r)
    logging.info(f"Pi {pi} finished database job")

def get_messages(pi, pipes):
    # TODO not sure if this is good
    for pipe in pipes:
        if pipe.poll():
            yield read_message(pi, pipe)


def run_worker(db, pi, pipes_read, pipes_write):
    # TODO param send array and recv array and loop while 1
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    logging.info(f"Starting process {pi}")
    #logging.info(f"pi {pi} updated database: {db[:]}")
    #print(db)
    time.sleep(1)
    #return

    requests_queue = RequestQueue(pi)
    clock = 0
    ko_count = 0
    ko_count_break_point = 2
    ack_required = len(pipes_write)
    ack_received = 0

    while True:
        # if not done send a new request
        if ko_count < ko_count_break_point:
            # craft request message
            msg = Message(Message.REQ, pi, clock)
            # broadcast request
            broadcast_message(pi, pipes_write, msg)
            # add my request to queue
            requests_queue.add(msg)
            logger.info(f"pi {pi} message queue updated: {requests_queue}")

        # just for sanity
        time.sleep(1)

        # wait for everyone to send ack for req -> can't go to K.O. before that
        # TODO request is not the first in the queue
        #while ack_received < ack_required or: # and not requests_queue.is_my_request_first():
        while True:
            # process all incoming messages from all pipes
            for msg in get_messages(pi, pipes_read):
                #logger.debug(f"pi {pi} just got message {msg}")

                # just a new request from somebody
                if msg.is_req():
                    # update clock
                    clock = max(clock, msg.sender_clock) + 1
                    logger.debug(f"pi {pi} clock updated: {clock}")
                    # save request to queueu
                    requests_queue.add(msg)
                    logger.info(f"pi {pi} message queue updated: {requests_queue}")
                    # send response ack
                    msg = Message(Message.ACK, pi, clock, msg.sender_pi)
                    broadcast_message(pi, pipes_write, msg)

                # recevied ack for my request
                if msg.is_ack(pi):
                    ack_received += 1
                    clock = max(clock,  msg.sender_clock) + 1
                    logger.debug(f"pi {pi} clock updated: {clock}")

                # received end messga for someone's exit from K.O.
                if msg.is_end():
                    clock = max(clock,  msg.sender_clock) + 1
                    logger.debug(f"pi {pi} clock updated: {clock}")
                    logger.warn(f"pi {pi} queue before remove {requests_queue} to remove {msg}")
                    requests_queue.remove(msg)
                    logger.warn(f"pi {pi} queue after remove {requests_queue}")
                    # TODO implement

            # check if I can go to K.O. now
            if ack_received == ack_required \
                and requests_queue.is_my_request_first():
                logger.debug(f"pi {pi} can go to K.O. because queue = {requests_queue} and ack recv = {ack_received} / {ack_required}")
                break  # break polling loop

            # TODO any else breaking points

        if ko_count < ko_count_break_point:
            logger.debug(f"pi {pi} just received all acks: clock = {clock} queue = {requests_queue} and entering KO")
            
            my_req = requests_queue.get_first()
            assert(my_req is not None)
            assert(my_req.sender_pi == pi)

            ko_count += 1
            ack_received = 0

            # do job
            run_database_job(db, pi, clock, ko_count)



            # TODO remove request from local
            logger.warn(f"pi {pi} queue before remove {requests_queue}")
            requests_queue.remove(my_req)
            logger.warn(f"pi {pi} queue after remove {requests_queue}")

            # TODO metadata from REQ must be sent in END

            # notify everyone upon exit
            msg = Message(Message.END, my_req.sender_pi, my_req.sender_clock)
            logger.debug(f"pi {pi} is sending END msg {msg}")
            broadcast_message(pi, pipes_write, msg)

        #





        # TODO go to KO

        #logger.debug(f"pi {pi} just received all acks: clock = {clock} queue = {requests_queue}")
        #time.sleep(1)
        #break

        # TODO INCREASE CLOCK UPON RECEIVEING ANY MESSAGE


        # TODO check if can go to KO


        # TODO nemoj slati jos dok ne dobije sve odgovore





            #broadcast_request(pi, clock, pipes_write)
            # TODO add your message to quueue

        # read responses from everyone
        """
        time.sleep(0.5)
        for msg in get_messages(pi, pipes_read):
            logger.debug(f"msg in pi {pi} => {msg}")
            if msg.is_req():
                requests_queue.add(msg)
                logger.debug(f"queue {pi}: {requests_queue}")
        """
             
             # TODO process response
             # 1 - new req
             # 2 - ack of my req
             # 3 - someone exited ko

        # check if can go in KO
        #for pipe in pipes_read:
        #    read_message(pi, pipe)

        #break
        #if random.randint(0, 3) == 0:
        #    break

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


    #time.sleep(2)
    #return


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

# TODO remove unused
def init_database():
    for i in range(NUM_WORKERS):
        pi = i + 1
        entry = DbEntry(pi, 0, 0)
        DATABASE.append(entry)
    logging.info(DATABASE)

def main():
    CustomManager.register('Database', Database)
    #manager = CustomManager()
    #db = manager.Database()
    #db = Array(Message, NUM_WORKERS)
    logging.debug('Starting main')

    with CustomManager() as manager:
        db = manager.Database()
        print(db)

        for i in range(NUM_WORKERS):
            pi = i + 1
            entry = DbEntry(pi, 0, 0)
            db.add(entry)
            #db[i] = entry
        logging.info(db)
        
        #return


        #return
        #print(db)
        pipes = []
        

        # init database
        #init_database()
        #for i in range(NUM_WORKERS):
        #    pi = i + 1
        #    entry = DbEntry(pi, 0, 0)
        #    db[i] = entry
        #logging.info(db[:])

        #return

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
            proc = Process(target=run_worker, args=(db, pi, pipes_read, pipes_write, ))
            logging.debug(f"proc: {proc} pi: {pi} pipes_read: {pipes_read} pipes_write: {pipes_write}")
            procs.append(proc)
            proc.start()

        for i in range(NUM_WORKERS):
            procs[i].join()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
