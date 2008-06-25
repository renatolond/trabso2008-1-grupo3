void le_socket(int socket, char *cmd, int cmdsize, string& msg)
{
    int n = 0;
    char buffer[512];
    int size;
    memset(cmd, 0, cmdsize);
    n = read(socket, cmd, 4);
    if ( n <= 0 )
    {
    }

    memset(buffer, 0, sizeof(buffer));
    n = read(socket, buffer, 4);
    if ( n <= 0 )
    {
    }
    sscanf(buffer, "%x", &size);

    msg = "";

    if ( size == 0 )
	return;

    while ( size > 500 )
    {
	memset(buffer, 0, sizeof(buffer));
	n = read(socket, buffer, 500);
	if ( n <= 0 )
	{
	}
	msg += buffer;
	size-=500;
    }

    memset(buffer, 0, sizeof(buffer));
    n = read(socket, buffer, size);

    if ( n <= 0 )
    {
    }

    msg += buffer;
}

void esc_socket(int socket, const char *cmd, int cmdsize, string msg)
{
    int n;
    char buffer[20];
    string command = string(cmd, cmdsize);

    sprintf(buffer, "%04x", msg.size());

    if ( strlen(buffer) > 4 )
    {
	exit(1);
    }

    while (command.size() < 4)
    {
	command += " ";
    }

    n = write(socket, command.c_str(), 4);

    if ( n <= 0 )
    {
    }

    n = write(socket, buffer, 4);

    n = write(socket, msg.c_str(), msg.size());
    
}
