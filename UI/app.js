// Polls ballcore's /api/state (contract: core/README.md) and draws it over the live stream.
(() => {
  const LOW_FPS = 10;     // error indicator below this
  const LOW_CONF = 0.2;   // flag detections below this

  const $ = (id) => document.getElementById(id);
  const img = $('Footage'), canvas = $('Overlay'), ctx = canvas.getContext('2d');

  // Opened from disk without the server: show the placeholder image.
  img.addEventListener('error', () => { if (!img.src.endsWith('Soccer.jpg')) img.src = 'Soccer.jpg'; });

  $('RecordBtn').onclick = () => fetch('/api/record?on=1');
  $('StopBtn').onclick = () => fetch('/api/record?on=0');

  function status(text, isError) {
    $('StatusLabel').textContent = text;
    $('StatusLabel').classList.toggle('error', isError);
  }

  function draw(s) {
    canvas.width = img.clientWidth;
    canvas.height = img.clientHeight;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (!s.camera.width) return;
    const k = canvas.width / s.camera.width;  // frame pixels -> displayed pixels

    ctx.strokeStyle = 'yellow';
    ctx.lineWidth = 2;
    ctx.beginPath();
    s.trail.forEach((p, i) => (i ? ctx.lineTo : ctx.moveTo).call(ctx, p.x * k, p.y * k));
    ctx.stroke();

    const b = s.ball;
    if (!b) return;
    const color = b.confidence < LOW_CONF ? 'red' : 'lime';
    ctx.strokeStyle = ctx.fillStyle = color;
    ctx.strokeRect(b.x * k, b.y * k, b.w * k, b.h * k);
    ctx.fillRect((b.x + b.w / 2) * k - 2, (b.y + b.h / 2) * k - 2, 4, 4);
    ctx.font = '14px sans-serif';
    ctx.fillText(Math.round(b.confidence * 100) + '%', b.x * k, b.y * k - 4);
  }

  function render(s) {
    const fps = s.fps.processing;
    if (!s.camera.ok) status('Camera disconnected', true);
    else status('FPS: ' + fps.toFixed(1) + (s.recording ? '  ● REC' : ''), fps < LOW_FPS);

    const a = s.analytics;
    $('Speed').textContent = a.speed_mps.toFixed(1) + ' m/s';
    $('Height').textContent = a.height_m.toFixed(1) + ' m';
    $('Zone').textContent = a.zone;
    $('ZoneTime').textContent = ((a.zone_time_s[a.zone] || 0) / 60).toFixed(1) + ' min';
    $('Confidence').textContent = s.ball ? Math.round(s.ball.confidence * 100) + '%' : '–';
    draw(s);
  }

  let up = true;  // the <img> already starts on /stream
  async function poll() {
    try {
      const s = await (await fetch('/api/state', { cache: 'no-store' })).json();
      if (!up) img.src = '/stream?t=' + Date.now();  // (re)connect the stream when the server appears
      up = true;
      render(s);
    } catch (e) {
      up = false;
      status('Server offline', true);
    }
    setTimeout(poll, 100);
  }
  poll();
})();
