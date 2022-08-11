#ifndef POPEN2_H
#define POPEN2_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>
#include <sstream>

#include <iostream>

using namespace std;

#define READ   0
#define WRITE  1
FILE * popen2(string command, string type, int & pid)
{
    pid_t child_pid;
    int fd[2];
    int rv = pipe(fd);
    if(rv == -1)
    	throw runtime_error("popen2: pipe failed!");

    if((child_pid = fork()) == -1)
    {
        perror("fork");
        exit(1);
    }

    /* child process */
    if (child_pid == 0)
    {
        if (type == "r")
        {
            close(fd[READ]);    //Close the READ end of the pipe since the child's fd is write-only
            dup2(fd[WRITE], 1); //Redirect stdout to pipe
        }
        else
        {
            close(fd[WRITE]);    //Close the WRITE end of the pipe since the child's fd is read-only
            dup2(fd[READ], 0);   //Redirect stdin to pipe
        }

        setpgid(child_pid, child_pid); //Needed so negative PIDs can kill children of /bin/sh
        execl("/bin/bash", "/bin/bash", "-c", command.c_str(), NULL);
        exit(0);
    }
    else
    {
        if (type == "r")
        {
            close(fd[WRITE]); //Close the WRITE end of the pipe since parent's fd is read-only
        }
        else
        {
            close(fd[READ]); //Close the READ end of the pipe since parent's fd is write-only
        }
    }

    pid = child_pid;

    if (type == "r")
    {
        return fdopen(fd[READ], "r");
    }

    return fdopen(fd[WRITE], "w");
}

void popen_bidirectional(string command, int & pid, FILE ** child_in, FILE ** child_out)
{
    pid_t child_pid;
    int fd_to_child[2];
    int fd_from_child[2];
    int rv = pipe(fd_to_child);
    int rv2 = pipe(fd_from_child);
    if(rv == -1 || rv2 == -1)
    	throw runtime_error("popen2: pipe failed!");

    if((child_pid = fork()) == -1)
    {
        perror("fork");
        exit(1);
    }

    /* child process */
    if (child_pid == 0)
    {
    	//close unused ends
    	close(fd_to_child[WRITE]);
    	close(fd_from_child[READ]);

        dup2(fd_to_child[READ], STDIN_FILENO);   //Redirect stdin to pipe
        dup2(fd_from_child[WRITE], STDOUT_FILENO); //Redirect stdout to pipe

        setpgid(child_pid, child_pid); //Needed so negative PIDs can kill children of /bin/sh
        execl("/bin/bash", "/bin/bash", "-c", command.c_str(), NULL);
        exit(0);
    }
    else
    {
    	close(fd_to_child[READ]);
    	close(fd_from_child[WRITE]);
    }
    pid = child_pid;

    *child_out = fdopen(fd_from_child[READ], "r");
    *child_in = fdopen(fd_to_child[WRITE], "w");
}

int pclose2(FILE * fp, pid_t pid)
{
    int stat;

    fclose(fp);
    while (waitpid(pid, &stat, 0) == -1)
    {
        if (errno != EINTR)
        {
            stat = -1;
            break;
        }
    }

    return stat;
}

#endif // POPEN2_H
