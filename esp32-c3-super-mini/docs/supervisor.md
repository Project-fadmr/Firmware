If `GPIO4` is pulled high, the supervisor should enter @bikeMode
else the supervisor should run in @carMode


<bikeMode>
the ESPNOW signal strenghts are compared, and when they're within {x} percent of each other, a single radar call is made, and a single frame is sent over espnow

<carMode>
continous radar samples are taken and streamed over espnow

<applies to both>
BSP should always be invoked whenever appropriate. the invoking pattern of BSP will depend on the specific mode

