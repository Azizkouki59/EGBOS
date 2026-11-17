import express from 'express';
import cors from 'cors';
import { GoogleGenAI } from '@google/genai';
import 'dotenv/config';

const app = express();

app.use(cors());
app.use(express.json());

// Check API key configuration
const apiKey = process.env.GEMINI_API_KEY;
if (!apiKey) {
  console.warn('⚠️  Warning: GEMINI_API_KEY is not set in environment variables.');
  console.warn('⚠️  Chat and analysis endpoints will return an error until GEMINI_API_KEY is configured.');
}

// Helper to get or instantiate GoogleGenAI client
function getAIClient() {
  const key = process.env.GEMINI_API_KEY;
  if (!key) {
    throw new Error('GEMINI_API_KEY is not configured on the server. Please set it in your environment variables.');
  }
  return new GoogleGenAI({ apiKey: key });
}

// ── Root & Health check ────────────────────────────────────────────────────────
app.get('/', (_req, res) => {
  res.json({
    name: 'egbos-backend',
    status: 'running',
    endpoints: ['/api/health', '/api/chat', '/api/analyze']
  });
});

app.get('/api/health', (_req, res) => {
  res.json({
    status: 'ok',
    geminiConfigured: Boolean(process.env.GEMINI_API_KEY),
    timestamp: new Date().toISOString()
  });
});

// ── Chat endpoint ─────────────────────────────────────────────────────────────
app.post('/api/chat', async (req, res) => {
  try {
    const { message, model = 'gemini-2.0-flash' } = req.body;

    if (!message || typeof message !== 'string') {
      return res.status(400).json({ error: 'message field is required and must be a string.' });
    }

    const ai = getAIClient();
    const response = await ai.models.generateContent({
      model,
      contents: message,
    });

    res.json({ reply: response.text });
  } catch (err) {
    console.error('Gemini error:', err.message);
    const statusCode = err.message.includes('GEMINI_API_KEY') ? 503 : 500;
    res.status(statusCode).json({ error: err.message ?? 'Internal server error' });
  }
});

// ── EGBOS market analysis endpoint ───────────────────────────────────────────
app.post('/api/analyze', async (req, res) => {
  try {
    const { symbol, data } = req.body;

    if (!symbol) {
      return res.status(400).json({ error: 'symbol field is required.' });
    }

    const ai = getAIClient();
    const prompt = `You are a quantitative market analyst. Analyze the following market data for ${symbol} and provide a concise structured summary including: trend direction, key support/resistance levels, notable patterns, and risk assessment.\n\nMarket data:\n${JSON.stringify(data, null, 2)}`;

    const response = await ai.models.generateContent({
      model: 'gemini-2.0-flash',
      contents: prompt,
    });

    res.json({ symbol, analysis: response.text });
  } catch (err) {
    console.error('Analysis error:', err.message);
    const statusCode = err.message.includes('GEMINI_API_KEY') ? 503 : 500;
    res.status(statusCode).json({ error: err.message ?? 'Internal server error' });
  }
});

// ── Start ─────────────────────────────────────────────────────────────────────
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`✅  EGBOS backend running on port ${PORT}`);
});
