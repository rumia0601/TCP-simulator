#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h> //exit
#include <string.h> //my_strcpy

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
	 are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
	 or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
	 (although some can be lost).
********************************************************************* */

/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1

#define BIDIRECTIONAL 1    /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

#define WINDOWSIZE 8 //size of window



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

struct event
{
	float evtime;           /* event time */
	int evtype;             /* event type code */
	int eventity;           /* entity where event occurs */
	struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
	struct event *prev;
	struct event *next;
};
struct event *evlist = NULL;   /* the event list */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg
{
	char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt
{
	int seqnum;
	int acknum;
	int checksum;
	char payload[20];
};

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

struct window
{
	char packet_text[WINDOWSIZE][20]; //single window can save 20 text (each text can save 20 char)
	int packet_number[WINDOWSIZE]; //unique, auto increment packet id
	int packet_count; //number of packet which are in window (20 means full, 0 means empty)
	int is_acked[WINDOWSIZE]; //1 if n-th packet is ACKed (if is_acked[0] == 1, window must be slided)
};
//window

int cur_seqA;
int timerA_is_working;
int timerA_seq; //seq of packet which started timer
int cur_ackA;
struct window windowA; //window for A (global structure)
//needed for A side

int cur_seqB;
int timerB_is_working;
int timerB_seq; //seq of packet which started timer
int cur_ackB;
struct window windowB; //window for B (global structure)
//needed for B side

int my_strcmp(dest, src)
char* dest;
char* src;
{
	for (int i = 0; i < 20; i++)
	{
		if (dest[i] > src[i])
			return 1;
		else if (dest[i] < src[i])
			return -1;
	}
	//same as strcmp_20

	return 0;
}

my_strcpy(dest, src)
char* dest;
char* src;
{
	for (int i = 0; i < 20; i++)
		dest[i] = src[i];
	//same as strcpy_20

	return;
}

show_window(p_window)
struct window* p_window;
{
	//printf("window of %p\n", p_window);
	//printf("packet_count %d\n", p_window->packet_count);

	//for (int i = 0; i < WINDOWSIZE; i++)
	//{
		//printf("%d ", i);
		//printf("packet_text : ");
		//for (int j = 0; j < 20; j++)
			//printf("%c", p_window->packet_text[i][j]);
		//printf(" ");
		//printf("packet_number : %d ", p_window->packet_number[i]);
		//printf("is_acked : %d\n", p_window->is_acked[i]);
	//}
	//printf("\n");

	return;
}
//show window (for debugging)

slide_window(p_window)
struct window* p_window;
{
	int shift_amount = 0;

	for (int i = 0; i < WINDOWSIZE; i++) //determine shift amount
	{
		if (p_window->is_acked[i] == 1)
			shift_amount = i + 1; //shift_amount will be 1 ~ WINDOWSIZE
	}

	if (shift_amount == 0) //no need to shift (return immediately)
		return;

	p_window->packet_count -= shift_amount; //decrease packet count of window

	for (int i = 0; i < shift_amount; i++) //repeat shift_amount times of slide by 1, so slide by shift_amount
	{
		for (int j = 0; j < WINDOWSIZE - 1; j++) //state of nth packet in window <= state of (n + 1)th packet in window
		{
			my_strcpy(p_window->packet_text[j], p_window->packet_text[j + 1]);
			p_window->packet_number[j] = p_window->packet_number[j + 1];
			p_window->is_acked[j] = p_window->is_acked[j + 1];
		}

		my_strcpy(p_window->packet_text[WINDOWSIZE - 1], "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");
		p_window->packet_number[WINDOWSIZE - 1] = -2;
		p_window->is_acked[WINDOWSIZE - 1] = 0;
		//state of last packet in window (clear) 
	}

	return;
}
//slide window

int checksum(p_pkt)
struct pkt* p_pkt;
{
	int checksum = 0;

	checksum = p_pkt->seqnum + p_pkt->acknum;
	for (int i = 0; i < 20; i++)
		checksum += p_pkt->payload[i];

	return checksum;
	//set checksum by add operation
}
//checksum function (with add operation)

//input of A_output = message(struct msg)
//output of A_output = calling for tolayer3(int AorB, struct pkt packet)
/* called from layer 5, passed the data to be sent to other side */
A_output(message)
struct msg message;
{
	if (windowA.packet_count == WINDOWSIZE)
	{
		printf("A output : Buffer is full. Drop the message.\n");
		return; //exeption for full window (discard message from layer 5)
	}

	struct pkt packet;
	packet.seqnum = cur_seqA; //seqnum is 0, 1, 2 ... 38, 39, 0 ... (circular)
	if (cur_seqA == 2 * WINDOWSIZE - 1) //2 * WINDOWSIZE - 1
		cur_seqA = 0;
	else //0 <= cur_seqA <= 2 * WINDOWSIZE - 2
		cur_seqA++;
	packet.acknum = cur_ackA; //set acknum

	for (int i = 0; i < 20; i++)
		packet.payload[i] = '\0';
	my_strcpy(packet.payload, message.data); //set payload by my_strcpy

	packet.checksum = checksum(&packet); //set checksum by add operation

	int index = windowA.packet_count;
	my_strcpy(windowA.packet_text[index], packet.payload);
	windowA.packet_number[index] = packet.seqnum;
	windowA.packet_count++;
	//store current packet into window with selective information

	if (timerA_is_working == 0) //when timer is not working
	{
		timerA_is_working = 1;

		timerA_seq = windowA.packet_number[0];
		//set seq which triggered timer

		starttimer(A, 2 * lambda); //start timer
		printf("A output : Start timer.\n");
	}

	tolayer3(A, packet);
	printf("A output : Send packet with ACK (seq = %d)(ack = %d)\n", packet.seqnum, packet.acknum);

	show_window(&windowA);

	return;
}

//input of B_output = message(struct msg)
//output of B_output = none(void)
B_output(message)  /* need be completed only for extra credit */
struct msg message;
{
	if (windowB.packet_count == WINDOWSIZE)
	{
		printf("B output : Buffer is full. Drop the message.\n");
		return; //exeption for full window (discard message from layer 5)
	}

	struct pkt packet;
	packet.seqnum = cur_seqB; //seqnum is 0, 1, 2 ... 38, 39, 0 ... (circular)
	if (cur_seqB == 2 * WINDOWSIZE - 1) //2 * WINDOWSIZE - 1
		cur_seqB = 0;
	else //0 <= cur_seqA <= 2 * WINDOWSIZE - 2
		cur_seqB++;
	packet.acknum = cur_ackB; //set acknum

	for (int i = 0; i < 20; i++)
		packet.payload[i] = '\0';
	my_strcpy(packet.payload, message.data); //set payload by my_strcpy

	packet.checksum = checksum(&packet); //set checksum by add operation

	int index = windowB.packet_count;
	my_strcpy(windowB.packet_text[index], packet.payload);
	windowB.packet_number[index] = packet.seqnum;
	windowB.packet_count++;
	//store current packet into window with selective information

	if (timerB_is_working == 0) //when timer is not working
	{
		timerB_is_working = 1;

		timerB_seq = windowB.packet_number[0];
		//set seq which triggered timer

		starttimer(B, 2 * lambda); //start timer
		printf("B output : Start timer.\n");
	}

	tolayer3(B, packet);
	printf("B output : Send packet with ACK (seq = %d)(ack = %d)\n", packet.seqnum, packet.acknum);

	show_window(&windowB);

	return;
}

//input of A_input = packet(struct pkt)
//output of A_output = calling for tolayer5(int AorB, char[20] datasent)
/* called from layer 3, when a packet arrives for layer 4 */
A_input(packet)
struct pkt packet;
{
	if (1) //process with acknum information
	{
		int checksum_of_A = 0;

		checksum_of_A = checksum(&packet); //check checksum by add operation (checksum for received data)

		if (checksum_of_A == packet.checksum) //checksum is correct (ACK)
		{
			int ack_key = packet.acknum;
			int ack_index = -1;

			printf("A input : Receive packet with ACK (seq = %d)(ack = %d)\n", packet.seqnum, packet.acknum);

			for (int i = 0; i < WINDOWSIZE; i++)
			{
				if (windowA.packet_number[i] == ack_key)
				{
					ack_index = i; //find packet which has same number as acknum
				}
			}

			if (ack_index == -1) //invalid acknum even if ACK is not corrupted (maybe late ACK)
			{
				;
			}

			else //without error
			{
				windowA.is_acked[ack_index] = 1; //mark certain packet is ACKed
			}
		}

		else //checksum_of_B != packet.checksum (checksum is incorrect (NAK))
		{
			printf("A input : Packet with ACK corrupted. Drop.\n\n");
			return; //do nothing (trigger timeout)
		}

		for (int i = WINDOWSIZE - 1; i > 0; i--)
		{
			if (windowA.is_acked[i] == 1) //if ith packet is acked
			{
				for (int j = 0; j < i; j++)
					windowA.is_acked[j] = 1; // 0th, 1th ... (i-1)th packet is assumed to be acked
			}
		}

		slide_window(&windowA);
		//slide window (0 or 8)

		show_window(&windowA);
	}

	if (1) //process with seqnum information
	{
		int checksum_of_A = 0;

		checksum_of_A = checksum(&packet); //check checksum by add operation (checksum for received data)

		if (checksum_of_A == packet.checksum) //checksum is correct (ACK)
		{
			if (cur_ackA == -1 && packet.seqnum == 0) //exception for 1st ack
			{
				cur_ackA = 0;

				tolayer5(A, packet.payload);

				printf("A input : Expected seq. A output will send packet with new ACK (seq = %d)(ack = %d)\n\n", cur_seqA, cur_ackA);
			}

			else if (packet.seqnum == cur_ackA + 1 || (packet.seqnum == 0 && cur_ackA == 2 * WINDOWSIZE - 1)) //received acknum is exactly what B expected
			{
				cur_ackA = packet.seqnum; //renew cur_ackA

				tolayer5(A, packet.payload);

				printf("A input : Expected seq. A output will send packet with new ACK (seq = %d)(ack = %d)\n\n", cur_seqA, cur_ackA);
			}

			else //received acknum is not what A expected
			{
				printf("A input : Not the expected seq. A output will send packet with duplicated ACK (seq = %d)(ack = %d)\n\n", cur_seqA, cur_ackA);
			}
		}

		else //checksum_of_A != packet.checksum (checksum is incorrect (NAK))
		{
			return;
		}
	}

	return;
}

/* called when A's timer goes off */
A_timerinterrupt()
{
	printf("A timerinterrupt : Timer is stopped.\n");

	timerA_is_working = 0;

	int found = 0;
	for (int i = 0; i < WINDOWSIZE - 1; i++)
	{
		if (windowA.packet_number[i] == timerA_seq)
		{
			found = 1; //timer ran out, but packet triggered timer is still in window (maybe loss)
		}
	}

	char text[WINDOWSIZE][20] = { '\0' };

	if (found == 1) //when have to go-back-n
	{
		windowA.packet_count = 0;

		for (int i = 0; i < WINDOWSIZE; i++) //go-back-n
		{
			if (windowA.packet_number[i] != -2) //if n-th packet of window is not empty
			{
				my_strcpy(text[i], windowA.packet_text[i]); //extract text from packet

				for (int j = 0; j < 20; j++)
					windowA.packet_text[i][j] = '\0';
				windowA.packet_number[i] = -2;
				windowA.is_acked[i] = 0;
				//clear packet

				if (cur_seqA == 0)
					cur_seqA = 2 * WINDOWSIZE - 1;
				else
					cur_seqA = cur_seqA - 1;
				//cur_set -= 1
			}
		}

		show_window(&windowA);

		printf("--------------------------------------------------A timerinterrupt : Resend packet begin.\n");
		for (int i = 0; i < WINDOWSIZE; i++)
		{
			struct msg message;
			my_strcpy(message.data, text[i]);

			if (my_strcmp(text[i], "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0") != 0) //when text is not empty (not empty packet)
			{
				A_output(message);
			}
		}
		printf("--------------------------------------------------A timerinterrupt : Resend packet end.\n");
		//reinsert packet with text

		show_window(&windowA);
	}

	else
	{
		if (windowA.packet_count == WINDOWSIZE) //when no need to GBN but window is full
		{
			timerA_is_working = 1;

			timerA_seq = windowA.packet_number[0];
			//set seq which triggered timer

			starttimer(A, 2 * lambda); //start timer
			printf("A timerinterrupt : Start timer.\n");
		}
	}

	printf("\n");
	return;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
A_init()
{
	cur_ackA = -1;
	cur_seqA = 0;
	timerA_is_working = 0;
	timerA_seq = 0;

	windowA.packet_count = 0;
	for (int i = 0; i < WINDOWSIZE; i++)
	{
		for (int j = 0; j < 20; j++)
			windowA.packet_text[i][j] = '\0';

		windowA.packet_number[i] = -2;
		windowA.is_acked[i] = 0;
	}
	//initialize field of windowA

	return;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

//input of B_input = packet(struct pkt)
//output of B_output = calling for tolayer5(int AorB, char[20] datasent)
/* called from layer 3, when a packet arrives for layer 4 at B*/
B_input(packet)
struct pkt packet;
{
	if (1) //process with acknum information
	{
		int checksum_of_B = 0;

		checksum_of_B = checksum(&packet); //check checksum by add operation (checksum for received data)

		if (checksum_of_B == packet.checksum) //checksum is correct (ACK)
		{
			int ack_key = packet.acknum;
			int ack_index = -1;

			printf("B input : Receive packet with ACK (seq = %d)(ack = %d)\n", packet.seqnum, packet.acknum);

			for (int i = 0; i < WINDOWSIZE; i++)
			{
				if (windowB.packet_number[i] == ack_key)
				{
					ack_index = i; //find packet which has same number as acknum
				}
			}

			if (ack_index == -1) //invalid acknum even if ACK is not corrupted (maybe late ACK)
			{
				;
			}

			else //without error
			{
				windowB.is_acked[ack_index] = 1; //mark certain packet us ACKed
			}
		}

		else //checksum_of_B != packet.checksum (checksum is incorrect (NAK))
		{
			printf("B input : Packet with ACK corrupted. Drop.\n\n");
			return; //do nothing (trigger timeout)
		}

		for (int i = WINDOWSIZE - 1; i > 0; i--)
		{
			if (windowB.is_acked[i] == 1) //if ith packet is acked
			{
				for (int j = 0; j < i; j++)
					windowB.is_acked[j] = 1; // 0th, 1th ... (i-1)th packet is assumed to be acked
			}
		}

		slide_window(&windowB);
		//slide window (0 or 8)

		show_window(&windowB);
	}

	if (1) //process with seqnum information
	{
		int checksum_of_B = 0;

		checksum_of_B = checksum(&packet); //check checksum by add operation (checksum for received data)

		if (checksum_of_B == packet.checksum) //checksum is correct (ACK)
		{
			if (cur_ackB == -1 && packet.seqnum == 0) //exception for 1st ack
			{
				cur_ackB = 0;

				tolayer5(B, packet.payload);

				printf("B input : Expected seq. B output will send packet with new ACK (seq = %d)(ack = %d)\n\n", cur_seqB, cur_ackB);
			}

			else if (packet.seqnum == cur_ackB + 1 || (packet.seqnum == 0 && cur_ackB == 2 * WINDOWSIZE - 1)) //received acknum is exactly what B expected
			{
				cur_ackB = packet.seqnum; //renew cur_ackB

				tolayer5(B, packet.payload);

				printf("B input : Expected seq. B output will send packet with new ACK (seq = %d)(ack = %d)\n\n", cur_seqB, cur_ackB);
			}

			else //received acknum is not what A expected
			{
				printf("B input : Not the expected seq. B output will send packet with duplicated ACK (seq = %d)(ack = %d)\n\n", cur_seqB, cur_ackB);
			}
		}

		else //checksum_of_B != packet.checksum (checksum is incorrect (NAK))
		{
			return;
		}
	}

	return;
}

/* called when B's timer goes off */
B_timerinterrupt()
{
	printf("B timerinterrupt : Timer is stopped.\n");

	timerB_is_working = 0;

	int found = 0;
	for (int i = 0; i < WINDOWSIZE - 1; i++)
	{
		if (windowB.packet_number[i] == timerB_seq)
		{
			found = 1; //timer ran out, but packet triggered timer is still in window (maybe loss)
		}
	}

	char text[WINDOWSIZE][20] = { '\0' };

	if (found == 1) //when have to go-back-n
	{
		windowB.packet_count = 0;

		for (int i = 0; i < WINDOWSIZE; i++) //go-back-n
		{
			if (windowB.packet_number[i] != -2) //if n-th packet of window is not empty
			{
				my_strcpy(text[i], windowB.packet_text[i]); //extract text from packet

				for (int j = 0; j < 20; j++)
					windowB.packet_text[i][j] = '\0';
				windowB.packet_number[i] = -2;
				windowB.is_acked[i] = 0;
				//clear packet

				if (cur_seqB == 0)
					cur_seqB = 2 * WINDOWSIZE - 1;
				else
					cur_seqB = cur_seqB - 1;
				//cur_seq -= 1
			}
		}

		show_window(&windowB);

		printf("--------------------------------------------------B timerinterrupt : Resend packet begin.\n");
		for (int i = 0; i < WINDOWSIZE; i++)
		{
			struct msg message;
			my_strcpy(message.data, text[i]);

			if (my_strcmp(text[i], "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0") != 0) //when text is not empty (not empty packet)
			{
				B_output(message);
			}
		}
		printf("--------------------------------------------------B timerinterrupt : Resend packet end.\n\n");
		//reinsert packet with text

		show_window(&windowB);
	}

	else
	{
		if (windowB.packet_count == WINDOWSIZE) //when no need to GBN but window is full
		{
			timerB_is_working = 1;

			timerB_seq = windowB.packet_number[0];
			//set seq which triggered timer

			starttimer(B, 2 * lambda); //start timer
			printf("B timerinterrupt : Start timer.\n");
		}
	}

	printf("\n");
	return;
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
B_init()
{
	cur_ackB = -1;
	cur_seqB = 0;
	timerB_is_working = 0;
	timerB_seq = 0;

	windowB.packet_count = 0;
	for (int i = 0; i < WINDOWSIZE; i++)
	{
		for (int j = 0; j < 20; j++)
			windowB.packet_text[i][j] = '\0';

		windowB.packet_number[i] = -2;
		windowB.is_acked[i] = 0;
	}
	//initialize field of windowA

	return;
}


/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
	and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
	interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/


main()
{
	struct event *eventptr;
	eventptr = NULL;
	//define and initialize

	struct msg  msg2give;
	for (int i = 0; i < 20; i++)
		msg2give.data[i] = '\0';
	//define and initialize

	struct pkt  pkt2give;
	pkt2give.seqnum = 0;
	pkt2give.acknum = 0;
	pkt2give.checksum = 0;
	for (int i = 0; i < 20; i++)
		pkt2give.payload[i] = '\0';
	//define and initialize

	int i, j;
	char c;

	init();
	A_init();
	B_init();
	//initializing both side

	while (1)
	{
		eventptr = evlist;            /* get next event to simulate */
		if (eventptr == NULL)
			goto terminate;
		evlist = evlist->next;        /* remove this event from event list */
		if (evlist != NULL)
			evlist->prev = NULL;
		if (TRACE >= 2)
		{
			printf("\nEVENT time: %f,", eventptr->evtime);
			printf("  type: %d", eventptr->evtype);
			if (eventptr->evtype == 0)
				printf(", timerinterrupt  ");
			else if (eventptr->evtype == 1)
				printf(", fromlayer5 ");
			else
				printf(", fromlayer3 ");
			printf(" entity: %d\n", eventptr->eventity);
		}
		time = eventptr->evtime;        /* update time to next event time */
		if (nsim == nsimmax)
			break;                        /* all done with simulation */
		if (eventptr->evtype == FROM_LAYER5) //something has arrived from application layer
		{
			generate_next_arrival();   /* set up future arrival */
			/* fill in msg to give with string of same letter */
			j = nsim % 26;
			for (i = 0; i < 20; i++)
				msg2give.data[i] = 97 + j;
			if (TRACE > 2)
			{
				printf("          MAINLOOP: data given to student: ");
				for (i = 0; i < 20; i++)
					printf("%c", msg2give.data[i]);
				printf("\n");
			}
			nsim++;
			if (eventptr->eventity == A)
				A_output(msg2give);
			else
				B_output(msg2give);
			//boxing that and send packet
		}
		else if (eventptr->evtype == FROM_LAYER3) //something has arrived from network layer
		{
			pkt2give.seqnum = eventptr->pktptr->seqnum;
			pkt2give.acknum = eventptr->pktptr->acknum;
			pkt2give.checksum = eventptr->pktptr->checksum;
			for (i = 0; i < 20; i++)
				pkt2give.payload[i] = eventptr->pktptr->payload[i];
			if (eventptr->eventity == A)      /* deliver packet by calling */
				A_input(pkt2give);            /* appropriate entity */
			else
				B_input(pkt2give);
			//unboxing that and receive packet
			free(eventptr->pktptr);          /* free the memory for packet */
		}
		else if (eventptr->evtype == TIMER_INTERRUPT) //excetpion for time out
		{
			if (eventptr->eventity == A)
				A_timerinterrupt();
			else
				B_timerinterrupt();
		}
		else
		{
			printf("INTERNAL PANIC: unknown event type \n");
		}
		free(eventptr);
	}

terminate:
	printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n", time, nsim);
}



init()                         /* initialize the simulator */
{
	int i;
	float sum, avg;
	float jimsrand();


	printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
	printf("Enter the number of messages to simulate:");
	scanf("%d", &nsimmax);
	printf("Enter packet loss probability [enter 0.0 for no loss]:");
	scanf("%f", &lossprob);
	printf("Enter packet corruption probability [0.0 for no corruption]:");
	scanf("%f", &corruptprob);
	printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
	scanf("%f", &lambda);
	printf("Enter TRACE:");
	scanf("%d", &TRACE);

	srand(9999);              /* init random number generator */
	sum = 0.0;                /* test random number generator for students */
	for (i = 0; i < 1000; i++)
		sum = sum + jimsrand();    /* jimsrand() should be uniform in [0,1] */
	avg = sum / 1000.0;
	if (avg < 0.25 || avg > 0.75)
	{
		printf("It is likely that random number generation on your machine\n");
		printf("is different from what this emulator expects.  Please take\n");
		printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
		exit(-1);
	}

	ntolayer3 = 0;
	nlost = 0;
	ncorrupt = 0;

	time = 0.0;                    /* initialize time to 0.0 */
	generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
	double mmm = 32767;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
	//range of rand() is 0 ~ 32767 so mmm must be 32767
	float x;                   /* individual students may need to change mmm */
	x = rand() / mmm;            /* x should be uniform in [0,1] */
	return(x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

generate_next_arrival()
{
	double x, log(), ceil();
	struct event *evptr;
	char *malloc();
	float ttime;
	int tempint;

	if (TRACE > 2)
		printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

	x = lambda * jimsrand() * 2;  /* x is uniform on [0,2*lambda] */
							  /* having mean of lambda        */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime = time + x;
	evptr->evtype = FROM_LAYER5;
	if (BIDIRECTIONAL && (jimsrand() > 0.5))
		evptr->eventity = B;
	else
		evptr->eventity = A;
	insertevent(evptr);
}


insertevent(p)
struct event *p;
{
	struct event *q, *qold;

	if (TRACE > 2)
	{
		printf("            INSERTEVENT: time is %lf\n", time);
		printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
	}
	q = evlist;     /* q points to header of list in which p struct inserted */
	if (q == NULL)
	{   /* list is empty */
		evlist = p;
		p->next = NULL;
		p->prev = NULL;
	}
	else
	{
		for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
			qold = q;
		if (q == NULL)
		{   /* end of list */
			qold->next = p;
			p->prev = qold;
			p->next = NULL;
		}
		else if (q == evlist)
		{ /* front of list */
			p->next = evlist;
			p->prev = NULL;
			p->next->prev = p;
			evlist = p;
		}
		else
		{     /* middle of list */
			p->next = q;
			p->prev = q->prev;
			q->prev->next = p;
			q->prev = p;
		}
	}
}

printevlist()
{
	struct event *q;
	int i;
	printf("--------------\nEvent List Follows:\n");
	for (q = evlist; q != NULL; q = q->next)
	{
		printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype, q->eventity);
	}
	printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
	struct event *q, *qold;

	if (TRACE > 2)
		printf("          STOP TIMER: stopping timer at %f\n", time);
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
		{
			/* remove this event */
			if (q->next == NULL && q->prev == NULL)
				evlist = NULL;         /* remove first and only event on list */
			else if (q->next == NULL) /* end of list - there is one in front */
				q->prev->next = NULL;
			else if (q == evlist)
			{ /* front of list - there must be event after */
				q->next->prev = NULL;
				evlist = q->next;
			}
			else
			{     /* middle of list */
				q->next->prev = q->prev;
				q->prev->next = q->next;
			}
			free(q);
			return;
		}
	printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


starttimer(AorB, increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{

	struct event *q;
	struct event *evptr;
	char *malloc();

	if (TRACE > 2)
		printf("          START TIMER: starting timer at %f\n", time);
	/* be nice: check to see if timer is already started, if so, then  warn */
   /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
		{
			printf("Warning: attempt to start a timer that is already started\n");
			return;
		}

	/* create future event for when timer goes off */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime = time + increment;
	evptr->evtype = TIMER_INTERRUPT;
	evptr->eventity = AorB;
	insertevent(evptr);
}


/************************** TOLAYER3 ***************/
tolayer3(AorB, packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
	struct pkt *mypktptr;
	struct event *evptr, *q;
	char *malloc();
	float lastime, x, jimsrand();
	int i;


	ntolayer3++;

	/* simulate losses: */
	if (jimsrand() < lossprob)
	{
		nlost++;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being lost\n");
		return;
	}

	/* make a copy of the packet student just gave me since he/she may decide */
	/* to do something with the packet after we return back to him/her */
	mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
	mypktptr->seqnum = packet.seqnum;
	mypktptr->acknum = packet.acknum;
	mypktptr->checksum = packet.checksum;
	for (i = 0; i < 20; i++)
		mypktptr->payload[i] = packet.payload[i];
	if (TRACE > 2)
	{
		printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
			mypktptr->acknum, mypktptr->checksum);
		for (i = 0; i < 20; i++)
			printf("%c", mypktptr->payload[i]);
		printf("\n");
	}

	/* create future event for arrival of packet at the other side */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtype = FROM_LAYER3;   /* packet will pop out from layer3 */
	evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
	evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
  /* finally, compute the arrival time of packet at the other end.
	 medium can not reorder, so make sure packet arrives between 1 and 10
	 time units after the latest arrival time of packets
	 currently in the medium on their way to the destination */
	lastime = time;
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
			lastime = q->evtime;
	evptr->evtime = lastime + 1 + 9 * jimsrand();



	/* simulate corruption: */
	if (jimsrand() < corruptprob)
	{
		ncorrupt++;
		if ((x = jimsrand()) < .75)
			mypktptr->payload[0] = 'Z';   /* corrupt payload */
		else if (x < .875)
			mypktptr->seqnum = 999999;
		else
			mypktptr->acknum = 999999;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being corrupted\n");
	}

	if (TRACE > 2)
		printf("          TOLAYER3: scheduling arrival on other side\n");
	insertevent(evptr);
}

tolayer5(AorB, datasent)
int AorB;
char datasent[20];
{
	int i;
	if (TRACE > 2)
	{
		printf("          TOLAYER5: data received: ");
		for (i = 0; i < 20; i++)
			printf("%c", datasent[i]);
		printf("\n");
	}

}