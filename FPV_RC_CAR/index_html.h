static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>XIAO FPV RC Car</title>
  <style>
    body { 
      font-family: Arial; text-align: center; margin: 0; padding: 0; 
      background-color: #222; color: white; overflow: hidden; user-select: none;
      touch-action: none; /* Critical: Disables browser zoom/scroll */
    }
    #stream-container { 
      position: absolute; top: 0; left: 0; width: 100%; height: 100%; 
      z-index: -1; display: flex; align-items: center; justify-content: center;
      background-color: #000;
    }
    img { 
      width: auto; height: auto; max-width: 95%; max-height: 95%; 
      object-fit: contain; transform: rotate(180deg); 
    }

    /* Slider UI */
    .slider-container {
      position: absolute; top: 60px; width: 100%; display: flex; 
      justify-content: center; align-items: center; z-index: 10;
    }
    .slider-box {
      background: rgba(0, 0, 0, 0.5); padding: 10px 20px; border-radius: 20px; 
      width: 60%; max-width: 300px;
    }
    input[type=range] {
      width: 100%; height: 10px; border-radius: 5px; background: #d3d3d3; 
      outline: none; -webkit-appearance: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; appearance: none; width: 20px; height: 20px; 
      border-radius: 50%; background: #007bff; cursor: pointer;
    }
    p { margin: 5px 0 0 0; font-size: 14px; }

    /* Joysticks */
    .controls-layer {
      position: absolute; bottom: 40px; width: 100%; height: 150px; 
      display: flex; justify-content: space-between; padding: 0 50px; 
      box-sizing: border-box; pointer-events: none; 
    }
    .joystick-zone {
      width: 120px; height: 120px; background: rgba(255, 255, 255, 0.15); 
      border: 2px solid rgba(255, 255, 255, 0.4); border-radius: 50%; 
      position: relative; pointer-events: auto; 
    }
    .stick {
      width: 50px; height: 50px; background: #007bff; border-radius: 50%; 
      position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); 
      box-shadow: 0 0 10px rgba(0,0,0,0.5); transition: transform 0.1s;
    }
    h2 { position: absolute; top: 10px; width: 100%; text-shadow: 1px 1px 2px black; pointer-events: none; margin: 0; padding-top: 10px; }
  </style>
</head>
<body>
  <h2>FPV RC Car</h2>
  <div class="slider-container">
    <div class="slider-box">
      <input type="range" min="50" max="255" value="255" id="limitRange" oninput="updateLimit(this.value)">
      <p>Max Power: <span id="limitVal">100%</span></p>
    </div>
  </div>
  
  <div id="stream-container"><img src="" id="photo" onload="this.style.display='block'"></div>
  
  <div class="controls-layer">
    <div id="joy-left" class="joystick-zone"><div class="stick" id="stick-left"></div></div>
    <div id="joy-right" class="joystick-zone"><div class="stick" id="stick-right"></div></div>
  </div>

<script>
  const JOYSTICK_RADIUS = 60; 
  const SEND_INTERVAL = 100;  
  let lastCmdTime = 0;
  let maxPowerLimit = 255; 

  window.onload = function() { document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream"; }

  function updateLimit(val) {
    maxPowerLimit = parseInt(val);
    const percentage = Math.round((maxPowerLimit / 255) * 100);
    document.getElementById("limitVal").innerText = percentage + "%";
  }

  // --- MULTI-TOUCH JOYSTICK ENGINE (FIXED FOR STICKINESS) ---
  function setupJoystick(zoneId, stickId, type) {
    const zone = document.getElementById(zoneId);
    const stick = document.getElementById(stickId);
    
    let currentTouchId = null;
    let startX, startY;

    // --- TOUCH START ---
    const start = (e) => {
      e.preventDefault();
      if (currentTouchId !== null) return; // Already active

      const touch = e.changedTouches[0]; 
      currentTouchId = touch.identifier; // Lock this finger ID

      const rect = zone.getBoundingClientRect();
      startX = rect.left + rect.width / 2;
      startY = rect.top + rect.height / 2;
      
      stick.style.transition = 'none'; 
      updateStick(touch.clientX, touch.clientY);

      // BIND TO WINDOW: This ensures we track the finger even if it leaves the circle
      window.addEventListener('touchmove', move, {passive: false});
      window.addEventListener('touchend', end, {passive: false});
      window.addEventListener('touchcancel', end, {passive: false});
    };

    // --- TOUCH MOVE ---
    const move = (e) => {
      e.preventDefault();
      for (let i = 0; i < e.changedTouches.length; i++) {
        if (e.changedTouches[i].identifier === currentTouchId) {
          updateStick(e.changedTouches[i].clientX, e.changedTouches[i].clientY);
          break; 
        }
      }
    };

    // --- TOUCH END ---
    const end = (e) => {
      e.preventDefault();
      for (let i = 0; i < e.changedTouches.length; i++) {
        if (e.changedTouches[i].identifier === currentTouchId) {
          // Finger Lifted
          currentTouchId = null; 
          stick.style.transition = 'transform 0.2s';
          stick.style.transform = `translate(-50%, -50%)`;
          
          // SAFETY: Send stop command 3 times to guarantee it stops
          sendSafeStop(type);

          // Remove Window Listeners
          window.removeEventListener('touchmove', move);
          window.removeEventListener('touchend', end);
          window.removeEventListener('touchcancel', end);
          break;
        }
      }
    };

    // Helper: Move Stick Visuals & Send Data
    function updateStick(clientX, clientY) {
      let dx = clientX - startX;
      let dy = clientY - startY;
      
      const distance = Math.sqrt(dx*dx + dy*dy);
      
      if (distance > JOYSTICK_RADIUS) {
        const ratio = JOYSTICK_RADIUS / distance;
        dx *= ratio; dy *= ratio;
      }

      stick.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;

      const now = Date.now();
      if (now - lastCmdTime > SEND_INTERVAL) {
        processJoystickData(type, dx, dy);
        lastCmdTime = now;
      }
    }

    zone.addEventListener('touchstart', start, {passive: false});
    zone.addEventListener('mousedown', (e) => {
      // Mouse support for testing (Simple version)
      startX = e.clientX; startY = e.clientY;
      // Note: Full mouse drag logic omitted for brevity as touch is priority
    });
  }

  function sendSafeStop(type) {
    const cmd = (type === 'throttle') ? "stop" : "straight";
    sendCommand(cmd);
    setTimeout(() => sendCommand(cmd), 50);
    setTimeout(() => sendCommand(cmd), 100);
  }

  function processJoystickData(type, dx, dy) {
    const normX = dx / JOYSTICK_RADIUS;
    const normY = dy / JOYSTICK_RADIUS;

    if (type === 'throttle') {
      const speed = Math.round(Math.abs(normY) * maxPowerLimit);
      if (speed > 20) { 
        fetch("/speed?value=" + speed);
        if (normY < -0.3) sendCommand("forward");
        else if (normY > 0.3) sendCommand("backward");
      } else { sendCommand("stop"); }
    } 
    else if (type === 'steering') {
      if (normX < -0.4) sendCommand("left");
      else if (normX > 0.4) sendCommand("right");
      else sendCommand("straight");
    }
  }

  function sendCommand(action) { fetch("/action?go=" + action); }

  setupJoystick('joy-left', 'stick-left', 'throttle');
  setupJoystick('joy-right', 'stick-right', 'steering');
</script>
</body>
</html>
)rawliteral";