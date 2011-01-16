# half jackal #

The setup is like so: 1 Arduino drives _2_ of the following
motor+motor controller setups. Motor with dual phase encoders is attached
to a "simple h-bridge". This gives us 2 encoder signals and current
information for each motor.

Motor speed is set via the usart interface, packets are framed in a hdlc
like manner, and CRC-CCITT protected.

Due to the physical layout of the robot 1 motor controler (half_jackal/hj)
will controll a pair of left/right wheels such that a hj controls the 2
front wheels and a second controls the 2 rear wheels.
