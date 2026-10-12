import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

// Custom metrics
const loginErrors = new Rate('login_errors');
const moveErrors = new Rate('move_errors');
const combatErrors = new Rate('combat_errors');
const tradeErrors = new Rate('trade_errors');

const loginDuration = new Trend('login_duration');
const moveDuration = new Trend('move_duration');
const combatDuration = new Trend('combat_duration');

const totalRequests = new Counter('total_requests');
const failedRequests = new Counter('failed_requests');

// Test configuration
export const options = {
  stages: [
    { duration: '30s', target: 50 },   // Ramp up to 50 users
    { duration: '1m', target: 100 },   // Stay at 100 users
    { duration: '2m', target: 250 },   // Ramp up to 250 users
    { duration: '3m', target: 500 },   // Peak load 500 users
    { duration: '2m', target: 250 },   // Ramp down
    { duration: '1m', target: 50 },    // Cool down
    { duration: '30s', target: 0 },    // Ramp to 0
  ],
  thresholds: {
    http_req_duration: ['p(95)<500', 'p(99)<1000'],
    http_req_failed: ['rate<0.01'],
    login_errors: ['rate<0.05'],
    move_errors: ['rate<0.02'],
    combat_errors: ['rate<0.05'],
  },
};

// Base URL - replace with your server URL
const BASE_URL = __ENV.BASE_URL || 'http://localhost:7777';
const WS_URL = __ENV.WS_URL || 'ws://localhost:7777';

// Test data
const TEST_ACCOUNTS = [];
for (let i = 1; i <= 1000; i++) {
  TEST_ACCOUNTS.push({
    username: `testuser${i}`,
    password: 'testpass123'
  });
}

function getRandomAccount() {
  return TEST_ACCOUNTS[Math.floor(Math.random() * TEST_ACCOUNTS.length)];
}

function makeRequest(method, url, payload = null, params = {}) {
  totalRequests.add(1);
  const startTime = new Date();
  
  let response;
  if (method === 'POST') {
    response = http.post(url, JSON.stringify(payload), {
      headers: { 'Content-Type': 'application/json' },
      ...params
    });
  } else {
    response = http.get(url, params);
  }
  
  const duration = new Date() - startTime;
  
  if (response.status >= 400) {
    failedRequests.add(1);
  }
  
  return { response, duration };
}

export function setup() {
  // Pre-create test accounts if needed
  console.log('Setting up test environment...');
  return { baseUrl: BASE_URL };
}

export default function(data) {
  const account = getRandomAccount();
  let sessionToken = null;
  let characterId = null;
  
  // 1. Login
  const loginStart = new Date();
  const loginRes = makeRequest('POST', `${data.baseUrl}/api/auth/login`, {
    username: account.username,
    password: account.password
  });
  loginDuration.add(loginRes.duration);
  
  const loginSuccess = check(loginRes.response, {
    'login status 200': (r) => r.status === 200,
    'has token': (r) => r.json('token') !== undefined
  });
  
  if (!loginSuccess) {
    loginErrors.add(1);
    return;
  }
  
  sessionToken = loginRes.response.json('token');
  characterId = loginRes.response.json('character_id');
  
  const authHeaders = {
    headers: { 'Authorization': `Bearer ${sessionToken}` }
  };
  
  // 2. Character select / enter world
  const charRes = makeRequest('POST', `${data.baseUrl}/api/character/enter`, {
    character_id: characterId
  }, { headers: { 'Authorization': `Bearer ${sessionToken}` } });
  
  check(charRes.response, {
    'enter world success': (r) => r.status === 200
  }) || failedRequests.add(1);
  
  // Simulate gameplay loop
  const iterations = Math.floor(Math.random() * 5) + 3; // 3-7 iterations
  
  for (let i = 0; i < iterations; i++) {
    const action = Math.random();
    
    if (action < 0.4) {
      // Movement
      const moveRes = makeRequest('POST', `${data.baseUrl}/api/character/move`, {
        x: Math.random() * 10000 - 5000,
        y: Math.random() * 10000 - 5000,
        z: 100
      }, { headers: { 'Authorization': `Bearer ${sessionToken}` } });
      
      moveDuration.add(moveRes.duration);
      check(moveRes.response, {
        'move success': (r) => r.status === 200
      }) || moveErrors.add(1);
      
    } else if (action < 0.7) {
      // Combat
      const combatRes = makeRequest('POST', `${data.baseUrl}/api/combat/attack`, {
        target_id: `mob_${Math.floor(Math.random() * 100)}`,
        skill_id: 'basic_attack'
      }, { headers: { 'Authorization': `Bearer ${sessionToken}` } });
      
      combatDuration.add(combatRes.duration);
      check(combatRes.response, {
        'combat success': (r) => r.status === 200
      }) || combatErrors.add(1);
      
    } else if (action < 0.85) {
      // Inventory action
      const invRes = makeRequest('GET', `${data.baseUrl}/api/inventory`, null, {
        headers: { 'Authorization': `Bearer ${sessionToken}` }
      });
      
      check(invRes.response, {
        'inventory success': (r) => r.status === 200
      }) || failedRequests.add(1);
      
    } else {
      // Trade / Social
      const socialRes = makeRequest('GET', `${data.baseUrl}/api/social/friends`, null, {
        headers: { 'Authorization': `Bearer ${sessionToken}` }
      });
      
      check(socialRes.response, {
        'social success': (r) => r.status === 200
      }) || tradeErrors.add(1);
    }
    
    // Think time between actions
    sleep(Math.random() * 2 + 0.5); // 0.5-2.5 seconds
  }
  
  // Logout
  makeRequest('POST', `${data.baseUrl}/api/auth/logout`, null, {
    headers: { 'Authorization': `Bearer ${sessionToken}` }
  });
  
  // Think time between sessions
  sleep(Math.random() * 5 + 1); // 1-6 seconds
}

export function teardown(data) {
  console.log('Test completed');
}