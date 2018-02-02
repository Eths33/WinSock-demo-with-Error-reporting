# WinSock-demo-with-Error-reporting
Socket demo with Error reporting, making a connection and streaming data

Working socket demo with error reporting as of VS2015 x64. Sharing this because when I was making samples there
were limited working samples for my needs, and it was a bit hard to find all of the error reporting code.
-Greg Gutmann

The terms client and sever are used loosly: the Server is the code that starts first and waits for a connection. 
For this code the sever is the main receiver and the client is the main sender.
The code connects exchanges parameters - starts the main data sending loop - after a given time the "ClientFile"
sets a flag for the connection to be closed. 
There are many print lines commented out, they can be uncommented to watch more of what is going on in the CMD.

<a rel="license" href="http://creativecommons.org/licenses/by/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by/4.0/">Creative Commons Attribution 4.0 International License</a>.
This license alows distribution, remix, tweak, and building upon the work, even commercially, as long as credit is given for the original creation. 
