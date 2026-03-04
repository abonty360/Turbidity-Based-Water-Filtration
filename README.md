# Turbidity-Based-Water-Filtration
An Arduino-based project which measures turbidity of a water sample and performs filtration based on turbidity value.
# Components
 1. Arduino UNO
 2. Turbidity Sensor: Gravity Arduino Analog Turbidity Sensor
 3. DC motor pump (5-6V)
 4. LCD Display: 1602 16x2 Characters LCD Blue Display with Driver Module
 5. Misc.: Breadboard, MOSFET, Diode, battery, wires etc.
    
# Workflow
 1. Turbidity sensor submerged in water sample.
 2. If NTU is greater than 50, meaning water is cloudy/dirty, motor swithces on and pumps the water sample through the filter and into another empty container.
 3. Filtrered water is then pumped back into the first container to measure turbidity again.
 4. repeats until water is clean.

# Thresholds
1. 0<NTU<=50 : Clean Water
2. 50<NTU<300 : Cloudy Water
3. NTU> 300: Dirty Water
    
