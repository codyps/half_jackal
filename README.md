# half jackal #

The setup is like so: 1 Arduino drives _2_ of the following
motor+motor controller setups. Motor with dual phase encoders is attached
to a "simple h-bridge". This gives us 2 encoder signals and current
information for each motor.

Motor speed is set via the usart interface, packets are framed in a hdlc
like manner, and CRC-CCITT protected.

As this is indended to drive half of the motors on a robot, the same speed
is desired on both the attached motors (it is driving one side). A second
(identical) setup drives the opposite side (left vs right).

