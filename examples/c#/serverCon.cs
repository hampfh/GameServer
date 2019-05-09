using System;
using System.Text;
using System.Net;
using System.Net.Sockets;

namespace SocketConnection {
    class Program {
        public static void Main(string[] args) {
            const string host = "127.0.0.1";
            const int port = 15000;

            // Connect
            Client client = new Client();
            if (client.Connect(host, port) != 0) {
                return;
            }

            while (true) {
                string response = client.Receive();
                if (response.Length > 1)
                    Console.WriteLine(response);
                client.Send("This is a test");
            }
        }
    }
    class Client {
        private Socket sock;

        public int Connect(string host, int port) {
            Socket s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            Console.WriteLine("Establishing Connection to {0}", host);
            try {
                s.Connect(host, port);
            }
            catch (Exception e) {
                Console.WriteLine(e);
                return 1;
            }
            
            Console.WriteLine("Connection established");
            sock = s;
            return 0;
        }

        public int Send(string outgoing) {
            Byte[] bytesOut = Encoding.ASCII.GetBytes(outgoing);
            sock.Send(bytesOut, bytesOut.Length, 0);
            return 0;
        }

        public string Receive() {
            Byte[] bytesIn = new Byte[1024];
            int response = sock.Receive(bytesIn, bytesIn.Length, 0);
            string result = Encoding.ASCII.GetString(bytesIn, 0, response);

            return result;
        }
    }
}
