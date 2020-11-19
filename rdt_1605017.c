#include "rdt.c"

struct pkt last_sent_A;
struct pkt last_recieved_B;
#define FLIP(pktnum) (pktnum+1) % 2
#define TIMER_COUNT     18
#define FREE    0
#define BLOCK   1
int state = FREE;

struct buffer
{
    struct msg message;
    struct buffer *next, *prev;
};
struct buffer *head;
struct buffer *tail;
int msg_count_A = 0;
void init_list()
{
    head = tail = NULL;
}
void push_msg(struct msg message)
{
    struct buffer * buf = (struct buffer *) malloc(sizeof(struct buffer));
    buf->message = message;
    msg_count_A++;
    if (head == NULL && tail == NULL) 
    {
        head = tail = buf;
        head->next = head->prev = tail->next = tail->prev = NULL;
        return;
    }
    if (head == tail)
    {
        head->next = buf;
    }
    tail->next = buf;
    buf->prev = tail;
    tail = buf;
}
struct msg pop_msg()
{
    struct msg message = head->message;
    struct buffer *tmp = head;
    if (head == tail)
    {
        head = tail = NULL;
    }
    else 
    {
        head->next->prev = NULL;
        head = head->next;
    }
    free(tmp);
    tmp = NULL;
    msg_count_A--;
    return message;
}


/* calculate checksum from packet */
int calc_checksum(struct pkt * packet)
{
    int chksum = 0;
    if (packet != NULL)
    {
        if (packet->payload != NULL)
        {
            for (int i = 0; i < 20; i++)
                chksum += packet->payload[i];
        }
        chksum += packet->seqnum + packet->acknum;
    }
    return chksum;
}


/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
    if (state == BLOCK)
    {
        push_msg(message);
        DBG("msg=[%s] pushed to queue\n", message.data);
        return;
    }
    DBG("A recieved msg=[%s] from layer 5\n", message.data);

    struct pkt packet;
    packet.acknum = 0;
    packet.seqnum = FLIP(last_sent_A.seqnum);
    strcpy(packet.payload, message.data);
    packet.checksum = calc_checksum(&packet);
    last_sent_A = packet;
    starttimer(A, TIMER_COUNT);
    state = BLOCK;
    tolayer3(A, packet);
    DBG("A sent packet seq=[%d], checksum=[%d], payload=[%s]\n", packet.seqnum, packet.checksum, packet.payload);
}

/* need be completed only for extra credit */
void B_output(struct msg message)
{

}
int total = 0;
/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
    DBG("A recieved packet seqnum=%d, acknum=%d, checksum=%d, payload=[%s] from layer 3\n", 
            packet.seqnum, packet.acknum, packet.checksum, packet.payload);
    // fine packet
    if (calc_checksum(&packet) == packet.checksum)
    {
        // it is an ack
        if (packet.acknum == 1)
        {
            stoptimer(A);
            // last sent packet got corrupted
            if (packet.seqnum != last_sent_A.seqnum)
            {
                starttimer(A, TIMER_COUNT);
                tolayer3(A, last_sent_A);
                DBG("A re-sent packet seq=[%d], checksum=[%d], payload=[%s]\n", last_sent_A.seqnum, last_sent_A.checksum, last_sent_A.payload);
            }
            else 
            {
                DBG("msg=[%s] got acknowledged by reciever, id=%d\n", last_sent_A.payload, ++total);
                state = FREE;
                if (msg_count_A > 0) A_output(pop_msg());
            }
        }
    }
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
    starttimer(A, TIMER_COUNT);
    tolayer3(A, last_sent_A);
    DBG("A sent packet seq=[%d], checksum=[%d], payload=[%s] from timer interrupt\n", last_sent_A.seqnum, last_sent_A.checksum, last_sent_A.payload);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
    init_list();
    last_sent_A.acknum = 0;
    last_sent_A.seqnum = 1;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
    DBG("B recieved packet seqnum=%d, acknum=%d, checksum=%d, payload=[%s] from layer 3\n", 
            packet.seqnum, packet.acknum, packet.checksum, packet.payload);

    if (calc_checksum(&packet) == packet.checksum)
    {
        // non-duplicate packet
        if (packet.seqnum != last_recieved_B.seqnum)
        {
            tolayer5(B, packet.payload);
            last_recieved_B.seqnum = packet.seqnum;
            last_recieved_B.checksum = calc_checksum(&last_recieved_B);
        }
        tolayer3(B, last_recieved_B);
    }
    
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
    printf("  B_timerinterrupt: B doesn't have a timer. ignore.\n");
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
    last_recieved_B.seqnum = 1;
    last_recieved_B.acknum = 1;
}
