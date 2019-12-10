# Echo Client/Server example

This example demonstrates how to use the tcp library and provides an application template for new tcp server applications.

This demo requires the boost library to be installed.

Two programs are generated: `echoclient` and `echoserver`

Type `echoclient --help` for a list of command line options

The programs look for a global configuration file in the `/etc` directory (`/etc/echoclient.conf` or `/etc/echoserver.conf`) and loads those options first.

Then it looks for a user specific configuration file in the user's home directory named `.echoclient.conf` or  `.echoserver.conf` and loads those options, over-writing any global options.

Finally, any command line options will override the options provided in the configuration files.

The configuration files should containe key/value pairs in the format `key=value` where key is the same as the program command line option.