/*
	net_dgrm.c
	This is enables a simple IP banning mechanism

	$Header: /home/ozzie/Download/0000/uhexen2/hexen2/net_dgrm.c,v 1.35 2007-10-01 14:32:22 sezero Exp $
*/

#define BAN_TEST

#if defined(BAN_TEST)
#include "net_sys.h"
#endif	/* BAN_TEST */

#include "quakedef.h"
#include "net_dgrm.h"

// these two macros are to make the code more readable
#define sfunc	net_landrivers[sock->landriver]
#define dfunc	net_landrivers[net_landriverlevel]

static int net_landriverlevel;

/* statistic counters */
static int packetsSent = 0;
static int packetsReSent = 0;
static int packetsReceived = 0;
static int receivedDuplicateCount = 0;
static int shortPacketCount = 0;
static int droppedDatagrams;

static int myDriverLevel;

static struct
{
	unsigned int	length;
	unsigned int	sequence;
	byte			data[MAX_DATAGRAM];
} packetBuffer;

extern int m_return_state;
extern int m_state;
extern qboolean m_return_onerror;
extern char m_return_reason[32];


#ifdef DEBUG_BUILD
static char *StrAddr (struct qsockaddr *addr)
{
	static char buf[34];
	byte *p = (byte *)addr;
	int n;

	for (n = 0; n < 16; n++)
		sprintf (buf + n * 2, "%02x", *p++);
	return buf;
}
#endif


#ifdef BAN_TEST

static in_addr_t	banAddr = 0x00000000;
static in_addr_t	banMask = 0xffffffff;

static void NET_Ban_f (void)
{
	char	addrStr [32];
	char	maskStr [32];
	void	(*print)(unsigned int flg, const char *fmt, ...) __attribute__((format(printf,2,3)));

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = CON_Printf;
	}
	else
	{
		if (PR_GLOBAL_STRUCT(deathmatch))
			return;
		print = SV_ClientPrintf;
	}

	switch (Cmd_Argc ())
	{
	case 1:
		if (((struct in_addr *)&banAddr)->s_addr)
		{
			strcpy(addrStr, inet_ntoa(*(struct in_addr *)&banAddr));
			strcpy(maskStr, inet_ntoa(*(struct in_addr *)&banMask));
			print(_PRINT_NORMAL, "Banning %s [%s]\n", addrStr, maskStr);
		}
		else
			print(_PRINT_NORMAL, "Banning not active\n");
		break;

	case 2:
		if (q_strcasecmp(Cmd_Argv(1), "off") == 0)
			banAddr = 0x00000000;
		else
			banAddr = inet_addr(Cmd_Argv(1));
		banMask = 0xffffffff;
		break;

	case 3:
		banAddr = inet_addr(Cmd_Argv(1));
		banMask = inet_addr(Cmd_Argv(2));
		break;

	default:
		print(_PRINT_NORMAL, "BAN ip_address [mask]\n");
		break;
	}
}
#endif	// BAN_TEST


int Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	unsigned int	packetLen;
	unsigned int	dataLen;
	unsigned int	eom;

#ifdef DEBUG_BUILD
	if (data->cursize == 0)
		Sys_Error("%s: zero length message", __thisfunc__);

	if (data->cursize > NET_MAXMESSAGE)
		Sys_Error("%s: message too big: %u", __thisfunc__, data->cursize);

	if (sock->canSend == false)
		Sys_Error("%s: called with canSend == false", __thisfunc__);
#endif

	memcpy(sock->sendMessage, data->data, data->cursize);
	sock->sendMessageLength = data->cursize;

	if (data->cursize <= MAX_DATAGRAM)
	{
		dataLen = data->cursize;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong(packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong(sock->sendSequence++);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->canSend = false;

	if (sfunc.Write (sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;
	return 1;
}


static int SendMessageNext (qsocket_t *sock)
{
	unsigned int	packetLen;
	unsigned int	dataLen;
	unsigned int	eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM)
	{
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong(packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong(sock->sendSequence++);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write (sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;
	return 1;
}


static int ReSendMessage (qsocket_t *sock)
{
	unsigned int	packetLen;
	unsigned int	dataLen;
	unsigned int	eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM)
	{
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong(packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong(sock->sendSequence - 1);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write (sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsReSent++;
	return 1;
}


qboolean Datagram_CanSendMessage (qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext (sock);

	return sock->canSend;
}


qboolean Datagram_CanSendUnreliableMessage (qsocket_t *sock)
{
	return true;
}


int Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int	packetLen;

#ifdef DEBUG_BUILD
	if (data->cursize == 0)
		Sys_Error("%s: zero length message", __thisfunc__);

	if (data->cursize > MAX_DATAGRAM)
		Sys_Error("%s: message too big: %u", __thisfunc__, data->cursize);
#endif

	packetLen = NET_HEADERSIZE + data->cursize;

	packetBuffer.length = BigLong(packetLen | NETFLAG_UNRELIABLE);
	packetBuffer.sequence = BigLong(sock->unreliableSendSequence++);
	memcpy (packetBuffer.data, data->data, data->cursize);

	if (sfunc.Write (sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	packetsSent++;
	return 1;
}


int	Datagram_GetMessage (qsocket_t *sock)
{
	unsigned int	length;
	unsigned int	flags;
	int				ret = 0;
	struct qsockaddr readaddr;
	unsigned int	sequence;
	unsigned int	count;

	if (!sock->canSend)
		if ((net_time - sock->lastSendTime) > 1.0)
			ReSendMessage (sock);

	while (1)
	{
		length = sfunc.Read (sock->socket, (byte *)&packetBuffer, NET_DATAGRAMSIZE, &readaddr);

//	if ((rand() & 255) > 220)
//		continue;

		if (length == 0)
			break;

		if (length == -1)
		{
			Con_Printf("Read error\n");
			return -1;
		}

		if (sfunc.AddrCompare(&readaddr, &sock->addr) != 0)
		{
#ifdef DEBUG_BUILD
			Con_DPrintf("Forged packet received\n");
			Con_DPrintf("Expected: %s\n", StrAddr (&sock->addr));
			Con_DPrintf("Received: %s\n", StrAddr (&readaddr));
#endif
			continue;
		}

		if (length < NET_HEADERSIZE)
		{
			shortPacketCount++;
			continue;
		}

		length = BigLong(packetBuffer.length);
		flags = length & (~NETFLAG_LENGTH_MASK);
		length &= NETFLAG_LENGTH_MASK;

		if (flags & NETFLAG_CTL)
			continue;

		sequence = BigLong(packetBuffer.sequence);
		packetsReceived++;

		if (flags & NETFLAG_UNRELIABLE)
		{
			if (sequence < sock->unreliableReceiveSequence)
			{
				Con_DPrintf("Got a stale datagram\n");
				ret = 0;
				break;
			}
			if (sequence != sock->unreliableReceiveSequence)
			{
				count = sequence - sock->unreliableReceiveSequence;
				droppedDatagrams += count;
				Con_DPrintf("Dropped %u datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;

			length -= NET_HEADERSIZE;

			SZ_Clear (&net_message);
			SZ_Write (&net_message, packetBuffer.data, length);

			ret = 2;
			break;
		}

		if (flags & NETFLAG_ACK)
		{
			if (sequence != (sock->sendSequence - 1))
			{
				Con_DPrintf("Stale ACK received\n");
				continue;
			}
			if (sequence == sock->ackSequence)
			{
				sock->ackSequence++;
				if (sock->ackSequence != sock->sendSequence)
					Con_DPrintf("ack sequencing error\n");
			}
			else
			{
				Con_DPrintf("Duplicate ACK received\n");
				continue;
			}
			sock->sendMessageLength -= MAX_DATAGRAM;
			if (sock->sendMessageLength > 0)
			{
				memcpy(sock->sendMessage, sock->sendMessage+MAX_DATAGRAM, sock->sendMessageLength);
				sock->sendNext = true;
			}
			else
			{
				sock->sendMessageLength = 0;
				sock->canSend = true;
			}
			continue;
		}

		if (flags & NETFLAG_DATA)
		{
			packetBuffer.length = BigLong(NET_HEADERSIZE | NETFLAG_ACK);
			packetBuffer.sequence = BigLong(sequence);
			sfunc.Write (sock->socket, (byte *)&packetBuffer, NET_HEADERSIZE, &readaddr);

			if (sequence != sock->receiveSequence)
			{
				receivedDuplicateCount++;
				continue;
			}
			sock->receiveSequence++;

			length -= NET_HEADERSIZE;

			if (flags & NETFLAG_EOM)
			{
				SZ_Clear(&net_message);
				SZ_Write(&net_message, sock->receiveMessage, sock->receiveMessageLength);
				SZ_Write(&net_message, packetBuffer.data, length);
				sock->receiveMessageLength = 0;

				ret = 1;
				break;
			}

			memcpy(sock->receiveMessage + sock->receiveMessageLength, packetBuffer.data, length);
			sock->receiveMessageLength += length;
			continue;
		}
	}

	if (sock->sendNext)
		SendMessageNext (sock);

	return ret;
}


static void PrintStats(qsocket_t *s)
{
	Con_Printf("canSend = %4u   \n", s->canSend);
	Con_Printf("sendSeq = %4u   ", s->sendSequence);
	Con_Printf("recvSeq = %4u   \n", s->receiveSequence);
	Con_Printf("\n");
}

static void NET_Stats_f (void)
{
	qsocket_t	*s;

	if (Cmd_Argc () == 1)
	{
		Con_Printf("unreliable messages sent   = %i\n", unreliableMessagesSent);
		Con_Printf("unreliable messages recv   = %i\n", unreliableMessagesReceived);
		Con_Printf("reliable messages sent     = %i\n", messagesSent);
		Con_Printf("reliable messages received = %i\n", messagesReceived);
		Con_Printf("packetsSent                = %i\n", packetsSent);
		Con_Printf("packetsReSent              = %i\n", packetsReSent);
		Con_Printf("packetsReceived            = %i\n", packetsReceived);
		Con_Printf("receivedDuplicateCount     = %i\n", receivedDuplicateCount);
		Con_Printf("shortPacketCount           = %i\n", shortPacketCount);
		Con_Printf("droppedDatagrams           = %i\n", droppedDatagrams);
	}
	else if (strcmp(Cmd_Argv(1), "*") == 0)
	{
		for (s = net_activeSockets; s; s = s->next)
			PrintStats(s);
		for (s = net_freeSockets; s; s = s->next)
			PrintStats(s);
	}
	else
	{
		for (s = net_activeSockets; s; s = s->next)
		{
			if (q_strcasecmp(Cmd_Argv(1), s->address) == 0)
				break;
		}

		if (s == NULL)
		{
			for (s = net_freeSockets; s; s = s->next)
			{
				if (q_strcasecmp(Cmd_Argv(1), s->address) == 0)
					break;
			}
		}

		if (s == NULL)
			return;

		PrintStats(s);
	}
}


static qboolean testInProgress = false;
static int		testPollCount;
static int		testDriver;
static int		testSocket;

static void Test_Poll (void *);
static PollProcedure	testPollProcedure = {NULL, 0.0, Test_Poll};

static void Test_Poll (void *unused)
{
	struct qsockaddr clientaddr;
	int		control;
	int		len;
	char	name[32];
	char	address[64];
	int		colors;
	int		frags;
	int		connectTime;
	byte	playerNumber;

	net_landriverlevel = testDriver;

	while (1)
	{
		len = dfunc.Read (testSocket, net_message.data, net_message.maxsize, &clientaddr);
		if (len < sizeof(int))
			break;

		net_message.cursize = len;

		MSG_BeginReading ();
		control = BigLong(*((int *)net_message.data));
		MSG_ReadLong();
		if (control == -1)
			break;
		if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
			break;
		if ((control & NETFLAG_LENGTH_MASK) != len)
			break;

		if (MSG_ReadByte() != CCREP_PLAYER_INFO)
			Sys_Error("Unexpected repsonse to Player Info request\n");

		playerNumber = MSG_ReadByte();
		strcpy(name, MSG_ReadString());
		colors = MSG_ReadLong();
		frags = MSG_ReadLong();
		connectTime = MSG_ReadLong();
		strcpy(address, MSG_ReadString());

		Con_Printf("%s\n  frags:%3i  colors:%d %d  time:%d\n  %s\n", name, frags, colors >> 4, colors & 0x0f, connectTime / 60, address);
	}

	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure(&testPollProcedure, 0.1);
	}
	else
	{
		dfunc.Close_Socket(testSocket);
		testInProgress = false;
	}
}

static void Test_f (void)
{
	char	*host;
	int		n;
	int		max = MAX_CLIENTS;
	struct qsockaddr sendaddr;

	if (testInProgress)
		return;

	host = Cmd_Argv (1);

	if (host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (q_strcasecmp (host, hostcache[n].name) == 0)
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				max = hostcache[n].maxusers;
				memcpy(&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		}

		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}

	if (net_landriverlevel == net_numlandrivers)
		return;

JustDoIt:
	testSocket = dfunc.Open_Socket(0);
	if (testSocket == -1)
		return;

	testInProgress = true;
	testPollCount = 20;
	testDriver = net_landriverlevel;

	for (n = 0; n < max; n++)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_PLAYER_INFO);
		MSG_WriteByte(&net_message, n);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	}
	SZ_Clear(&net_message);
	SchedulePollProcedure(&testPollProcedure, 0.1);
}


static qboolean test2InProgress = false;
static int		test2Driver;
static int		test2Socket;

static void Test2_Poll (void *);
static PollProcedure	test2PollProcedure = {NULL, 0.0, Test2_Poll};

static void Test2_Poll (void *unused)
{
	struct qsockaddr clientaddr;
	int		control;
	int		len;
	char	name[256];
	char	value[256];

	net_landriverlevel = test2Driver;
	name[0] = 0;

	len = dfunc.Read (test2Socket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		goto Reschedule;

	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		goto Error;
	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RULE_INFO)
		goto Error;

	strcpy(name, MSG_ReadString());
	if (name[0] == 0)
		goto Done;
	strcpy(value, MSG_ReadString());

	Con_Printf("%-16.16s  %-16.16s\n", name, value);

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
	MSG_WriteString(&net_message, name);
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (test2Socket, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear(&net_message);

Reschedule:
	SchedulePollProcedure(&test2PollProcedure, 0.05);
	return;

Error:
	Con_Printf("Unexpected repsonse to Rule Info request\n");
Done:
	dfunc.Close_Socket(test2Socket);
	test2InProgress = false;
	return;
}

static void Test2_f (void)
{
	char	*host;
	int		n;
	struct qsockaddr sendaddr;

	if (test2InProgress)
		return;

	host = Cmd_Argv (1);

	if (host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (q_strcasecmp (host, hostcache[n].name) == 0)
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				memcpy(&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		}

		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}

	if (net_landriverlevel == net_numlandrivers)
		return;

JustDoIt:
	test2Socket = dfunc.Open_Socket(0);
	if (test2Socket == -1)
		return;

	test2InProgress = true;
	test2Driver = net_landriverlevel;

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
	MSG_WriteString(&net_message, "");
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (test2Socket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear(&net_message);
	SchedulePollProcedure(&test2PollProcedure, 0.05);
}


int Datagram_Init (void)
{
	int	i, csock, num_inited;

	myDriverLevel = net_driverlevel;
	Cmd_AddCommand ("net_stats", NET_Stats_f);

	if (safemode || COM_CheckParm("-nolan"))
		return -1;

	num_inited = 0;
	for (i = 0; i < net_numlandrivers; i++)
	{
		csock = net_landrivers[i].Init ();
		if (csock == -1)
			continue;
		net_landrivers[i].initialized = true;
		net_landrivers[i].controlSock = csock;
		num_inited++;
	}

	if (num_inited == 0)
		return -1;

#ifdef BAN_TEST
	Cmd_AddCommand ("ban", NET_Ban_f);
#endif
	Cmd_AddCommand ("test", Test_f);
	Cmd_AddCommand ("test2", Test2_f);

	return 0;
}


void Datagram_Shutdown (void)
{
	int i;

//
// shutdown the lan drivers
//
	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
		{
			net_landrivers[i].Shutdown ();
			net_landrivers[i].initialized = false;
		}
	}
}


void Datagram_Close (qsocket_t *sock)
{
	sfunc.Close_Socket(sock->socket);
}


void Datagram_Listen (qboolean state)
{
	int i;

	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
			net_landrivers[i].Listen (state);
	}
}


static qsocket_t *Datagram_Reject (const char *message, int acceptsocket, struct qsockaddr *addr)
{
	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREP_REJECT);
	MSG_WriteString(&net_message, message);
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsocket, net_message.data, net_message.cursize, addr);
	SZ_Clear(&net_message);
	return NULL;
}

static qsocket_t *_Datagram_CheckNewConnections (void)
{
	struct qsockaddr clientaddr;
	struct qsockaddr newaddr;
	int			newsock;
	int			acceptsock;
	qsocket_t	*sock;
	qsocket_t	*s;
	int			len;
	int			command;
	int			control;
	int			ret;

	acceptsock = dfunc.CheckNewConnections();
	if (acceptsock == -1)
		return NULL;

	SZ_Clear(&net_message);

	len = dfunc.Read (acceptsock, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		return NULL;
	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		return NULL;
	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		return NULL;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		return NULL;

	command = MSG_ReadByte();
	if (command == CCREQ_SERVER_INFO)
	{
		if (strcmp(MSG_ReadString(), NET_NAME_ID) != 0)
			return NULL;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_SERVER_INFO);
		dfunc.GetSocketAddr(acceptsock, &newaddr);
		MSG_WriteString(&net_message, dfunc.AddrToString(&newaddr));
		MSG_WriteString(&net_message, hostname.string);
		MSG_WriteString(&net_message, sv.name);
		MSG_WriteByte(&net_message, net_activeconnections);
		MSG_WriteByte(&net_message, svs.maxclients);
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	if (command == CCREQ_PLAYER_INFO)
	{
		int			playerNumber;
		int			activeNumber;
		int			clientNumber;
		client_t	*client;

		playerNumber = MSG_ReadByte();
		activeNumber = -1;

		for (clientNumber = 0, client = svs.clients; clientNumber < svs.maxclients; clientNumber++, client++)
		{
			if (client->active)
			{
				activeNumber++;
				if (activeNumber == playerNumber)
					break;
			}
		}

		if (clientNumber == svs.maxclients)
			return NULL;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_PLAYER_INFO);
		MSG_WriteByte(&net_message, playerNumber);
		MSG_WriteString(&net_message, client->name);
		MSG_WriteLong(&net_message, client->colors);
		MSG_WriteLong(&net_message, (int)client->edict->v.frags);
		MSG_WriteLong(&net_message, (int)(net_time - client->netconnection->connecttime));
		MSG_WriteString(&net_message, client->netconnection->address);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);

		return NULL;
	}

	if (command == CCREQ_RULE_INFO)
	{
		char	*prevCvarName;
		cvar_t	*var;

		// find the search start location
		prevCvarName = MSG_ReadString();
		var = Cvar_FindVarAfter (prevCvarName, CVAR_SERVERINFO);

		// send the response
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_RULE_INFO);
		if (var)
		{
			MSG_WriteString(&net_message, var->name);
			MSG_WriteString(&net_message, var->string);
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);

		return NULL;
	}

	if (command != CCREQ_CONNECT)
		return NULL;

	if (strcmp(MSG_ReadString(), NET_NAME_ID) != 0)
		return NULL;

	if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		return Datagram_Reject("Incompatible version.\n", acceptsock, &clientaddr);

#ifdef BAN_TEST
	// check for a ban
	if (clientaddr.sa_family == AF_INET)
	{
		in_addr_t	testAddr;
		testAddr = ((struct sockaddr_in *)&clientaddr)->sin_addr.s_addr;
		if ((testAddr & banMask) == banAddr)
			return Datagram_Reject("You have been banned.\n", acceptsock, &clientaddr);
	}
#endif

	// see if this guy is already connected
	for (s = net_activeSockets; s; s = s->next)
	{
		if (s->driver != net_driverlevel)
			continue;
		ret = dfunc.AddrCompare(&clientaddr, &s->addr);
		if (ret >= 0)
		{
			// is this a duplicate connection reqeust?
			if (ret == 0 && net_time - s->connecttime < 2.0)
			{
				// yes, so send a duplicate reply
				SZ_Clear(&net_message);
				// save space for the header, filled in later
				MSG_WriteLong(&net_message, 0);
				MSG_WriteByte(&net_message, CCREP_ACCEPT);
				dfunc.GetSocketAddr(s->socket, &newaddr);
				MSG_WriteLong(&net_message, dfunc.GetSocketPort(&newaddr));
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
				SZ_Clear(&net_message);
				return NULL;
			}
			// it's somebody coming back in from a crash/disconnect
			// so close the old qsocket and let their retry get them back in
			NET_Close(s);
			return NULL;
		}
	}

	// allocate a QSocket
	sock = NET_NewQSocket ();
	if (sock == NULL)	// no room; try to let him know
		return Datagram_Reject("Server is full.\n", acceptsock, &clientaddr);

	// allocate a network socket
	newsock = dfunc.Open_Socket(0);
	if (newsock == -1)
	{
		NET_FreeQSocket(sock);
		return NULL;
	}

	// connect to the client
	if (dfunc.Connect (newsock, &clientaddr) == -1)
	{
		dfunc.Close_Socket(newsock);
		NET_FreeQSocket(sock);
		return NULL;
	}

	// everything is allocated, just fill in the details
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	strcpy(sock->address, dfunc.AddrToString(&clientaddr));

	// send him back the info about the server connection he has been allocated
	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREP_ACCEPT);
	dfunc.GetSocketAddr(newsock, &newaddr);
	MSG_WriteLong(&net_message, dfunc.GetSocketPort(&newaddr));
//	MSG_WriteString(&net_message, dfunc.AddrToString(&newaddr));
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear(&net_message);

	return sock;
}

qsocket_t *Datagram_CheckNewConnections (void)
{
	qsocket_t *ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_CheckNewConnections ()) != NULL)
				break;
		}
	}
	return ret;
}


static void _Datagram_SearchForHosts (qboolean xmit)
{
	int		ret;
	int		n;
	int		i;
	struct qsockaddr readaddr;
	struct qsockaddr myaddr;
	int		control;

	dfunc.GetSocketAddr (dfunc.controlSock, &myaddr);
	if (xmit)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, NET_NAME_ID);
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Broadcast(dfunc.controlSock, net_message.data, net_message.cursize);
		SZ_Clear(&net_message);
	}

	while ((ret = dfunc.Read (dfunc.controlSock, net_message.data, net_message.maxsize, &readaddr)) > 0)
	{
		if (ret < sizeof(int))
			continue;
		net_message.cursize = ret;

		// don't answer our own query
		if (dfunc.AddrCompare(&readaddr, &myaddr) >= 0)
			continue;

		// is the cache full?
		if (hostCacheCount == HOSTCACHESIZE)
			continue;

		MSG_BeginReading ();
		control = BigLong(*((int *)net_message.data));
		MSG_ReadLong();
		if (control == -1)
			continue;
		if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
			continue;
		if ((control & NETFLAG_LENGTH_MASK) != ret)
			continue;

		if (MSG_ReadByte() != CCREP_SERVER_INFO)
			continue;

		dfunc.GetAddrFromName(MSG_ReadString(), &readaddr);
		// search the cache for this server
		for (n = 0; n < hostCacheCount; n++)
		{
			if (dfunc.AddrCompare(&readaddr, &hostcache[n].addr) == 0)
				break;
		}

		// is it already there?
		if (n < hostCacheCount)
			continue;

		// add it
		hostCacheCount++;
		strcpy(hostcache[n].name, MSG_ReadString());
		strcpy(hostcache[n].map, MSG_ReadString());
		hostcache[n].users = MSG_ReadByte();
		hostcache[n].maxusers = MSG_ReadByte();
		if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{
			strcpy(hostcache[n].cname, hostcache[n].name);
			hostcache[n].cname[14] = 0;
			strcpy(hostcache[n].name, "*");
			strcat(hostcache[n].name, hostcache[n].cname);
		}
		memcpy(&hostcache[n].addr, &readaddr, sizeof(struct qsockaddr));
		hostcache[n].driver = net_driverlevel;
		hostcache[n].ldriver = net_landriverlevel;
		strcpy(hostcache[n].cname, dfunc.AddrToString(&readaddr));

		// check for a name conflict
		for (i = 0; i < hostCacheCount; i++)
		{
			if (i == n)
				continue;
			if (q_strcasecmp (hostcache[n].name, hostcache[i].name) == 0)
			{
				i = strlen(hostcache[n].name);
				if (i < 15 && hostcache[n].name[i-1] > '8')
				{
					hostcache[n].name[i] = '0';
					hostcache[n].name[i+1] = 0;
				}
				else
					hostcache[n].name[i-1]++;

				i = -1;
			}
		}
	}
}

void Datagram_SearchForHosts (qboolean xmit)
{
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (hostCacheCount == HOSTCACHESIZE)
			break;
		if (net_landrivers[net_landriverlevel].initialized)
			_Datagram_SearchForHosts (xmit);
	}
}


static qsocket_t *_Datagram_Connect (char *host)
{
	struct qsockaddr sendaddr;
	struct qsockaddr readaddr;
	qsocket_t	*sock;
	int			newsock;
	int			ret;
	int			reps;
	double		start_time;
	int			control;
	char		*reason;

	// see if we can resolve the host name
	if (dfunc.GetAddrFromName(host, &sendaddr) == -1)
		return NULL;

	newsock = dfunc.Open_Socket (0);
	if (newsock == -1)
		return NULL;

	sock = NET_NewQSocket ();
	if (sock == NULL)
		goto ErrorReturn2;
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	// connect to the host
	if (dfunc.Connect (newsock, &sendaddr) == -1)
		goto ErrorReturn;

	// send the connection request
	Con_Printf("trying...\n");
	SCR_UpdateScreen ();
	start_time = net_time;

	for (reps = 0; reps < 3; reps++)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_CONNECT);
		MSG_WriteString(&net_message, NET_NAME_ID);
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (newsock, net_message.data, net_message.cursize, &sendaddr);
		SZ_Clear(&net_message);
		do
		{
			ret = dfunc.Read (newsock, net_message.data, net_message.maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0)
			{
				// is it from the right place?
				if (sfunc.AddrCompare(&readaddr, &sendaddr) != 0)
				{
#ifdef DEBUG_BUILD
					Con_Printf("wrong reply address\n");
					Con_Printf("Expected: %s\n", StrAddr (&sendaddr));
					Con_Printf("Received: %s\n", StrAddr (&readaddr));
					SCR_UpdateScreen ();
#endif
					ret = 0;
					continue;
				}

				if (ret < sizeof(int))
				{
					ret = 0;
					continue;
				}

				net_message.cursize = ret;
				MSG_BeginReading ();

				control = BigLong(*((int *)net_message.data));
				MSG_ReadLong();
				if (control == -1)
				{
					ret = 0;
					continue;
				}
				if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
				{
					ret = 0;
					continue;
				}
				if ((control & NETFLAG_LENGTH_MASK) != ret)
				{
					ret = 0;
					continue;
				}
			}
		}
		while (ret == 0 && (SetNetTime() - start_time) < 2.5);

		if (ret)
			break;

		Con_Printf("still trying...\n");
		SCR_UpdateScreen ();
		start_time = SetNetTime();
	}

	if (ret == 0)
	{
		reason = "No Response";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	if (ret == -1)
	{
		reason = "Network Error";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	ret = MSG_ReadByte();
	if (ret == CCREP_REJECT)
	{
		reason = MSG_ReadString();
		Con_Printf(reason);
		q_strlcpy(m_return_reason, reason, sizeof(m_return_reason));
		goto ErrorReturn;
	}

	if (ret == CCREP_ACCEPT)
	{
		memcpy(&sock->addr, &sendaddr, sizeof(struct qsockaddr));
		dfunc.SetSocketPort (&sock->addr, MSG_ReadLong());
	}
	else
	{
		reason = "Bad Response";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	dfunc.GetNameFromAddr (&sendaddr, sock->address);

	Con_Printf ("Connection accepted\n");
	sock->lastMessageTime = SetNetTime();

	// switch the connection to the specified address
	if (dfunc.Connect (newsock, &sock->addr) == -1)
	{
		reason = "Connect to Game failed";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	m_return_onerror = false;
	return sock;

ErrorReturn:
	NET_FreeQSocket(sock);
ErrorReturn2:
	dfunc.Close_Socket(newsock);
	if (m_return_onerror)
	{
		key_dest = key_menu;
		m_state = m_return_state;
		m_return_onerror = false;
	}
	return NULL;
}

qsocket_t *Datagram_Connect (char *host)
{
	qsocket_t *ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_Connect (host)) != NULL)
				break;
		}
	}
	return ret;
}

