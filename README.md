Vkax Core staging tree 100.0
===========================

|CI|master|develop|
|-|-|-|
|Gitlab|[![Build Status](https://gitlab.com/dashpay/dash/badges/master/pipeline.svg)](https://gitlab.com/dashpay/dash/-/tree/master)|[![Build Status](https://gitlab.com/dashpay/dash/badges/develop/pipeline.svg)](https://gitlab.com/dashpay/dash/-/tree/develop)|

Website: TBA

Explorer: https://x.butkoin.com/vkax


What is Vkax?
-------------

Vkax is an experimental digital currency that enables instant, private
payments to anyone, anywhere in the world. Vkax uses peer-to-peer technology
to operate with no central authority: managing transactions and issuing money
are carried out collectively by the network. Vkax Core is the name of the open
source software which enables the use of this currency.

License
-------

Dash Core and Vkax project core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

This fork proposes to become the new `master` branch. Development is ongoing and caution is advised before proceeding to interact with this coin, for those who do not know what they are doing. 

Development
------------

If you believe you have an idea or change you would like to make, please use the 'issues' tab to create a discussion!

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and macOS, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

