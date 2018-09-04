# Windows ASIO Support

In order to build ASIO support into Pd on Windows, you need to download the
ASIO sources from Steinberg directly. Their license does not let us
redistribute their source files.

Install the ASIO SDK by doing the following:

1. Download the ASIO SDK: https://www.steinberg.net/en/company/developer.html
2. Uncompress asiosdk2.3.zip (or higher) into this directory
3. remove the version number so that you get pure-data/asio/ASIOSDK

Now build Pd and it should include ASIO as one of the audio backends.
