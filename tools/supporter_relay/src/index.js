// Cloudflare Worker: relays Ko-fi (/kofi) and Buy Me a Coffee (/bmc) webhooks to the
// Discord #supporters channel through a channel webhook. Amounts are never shown; supporters
// who chose to stay private are posted as "Someone" without their message.
//
// Secrets (wrangler secret put ...): DISCORD_WEBHOOK_URL, KOFI_VERIFICATION_TOKEN, BMC_WEBHOOK_SECRET.

const ICON = 'https://raw.githubusercontent.com/freefrank/LostOdysseyRecomp/main/assets/lost-odyssey-recomp.png';
const PAGES = { 'Ko-fi': 'https://ko-fi.com/dotslash', 'Buy Me a Coffee': 'https://buymeacoffee.com/dotslash' };
const COLORS = { 'Ko-fi': 0x29abe0, 'Buy Me a Coffee': 0xffdd00 };

export default {
  async fetch(request, env) {
    const { pathname } = new URL(request.url);
    const handler = request.method === 'POST' && { '/kofi': fromKofi, '/bmc': fromBmc }[pathname];
    if (!handler) return new Response('Not found', { status: 404 });
    let post;
    try {
      post = await handler(request, env);
    } catch {
      return new Response('Bad request', { status: 400 });
    }
    if (post === undefined) return new Response('Forbidden', { status: 403 });
    if (post) {
      // A failed post returns 502 so the platform retries the delivery.
      const res = await fetch(env.DISCORD_WEBHOOK_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(discordMessage(post)),
      });
      if (!res.ok) return new Response('Discord rejected the message', { status: 502 });
    }
    return new Response('OK');
  },
};

// Returns a post, null for events that are not announced, or undefined when verification fails.
async function fromKofi(request, env) {
  const data = JSON.parse((await request.formData()).get('data') || '{}');
  if (!env.KOFI_VERIFICATION_TOKEN || data.verification_token !== env.KOFI_VERIFICATION_TOKEN) return undefined;
  if (data.type === 'Subscription' && !data.is_first_subscription_payment) return null; // monthly renewals
  const visible = data.is_public !== false;
  const what = {
    Donation: 'bought a coffee',
    Subscription: 'became a monthly supporter',
    'Shop Order': 'bought something from the shop',
    Commission: 'ordered a commission',
  }[data.type] || 'supported the project';
  return { platform: 'Ko-fi', who: visible && data.from_name, what, message: visible && data.message };
}

async function fromBmc(request, env) {
  const body = await request.text();
  const signature = (request.headers.get('x-signature-sha256') || '').toLowerCase();
  if (!env.BMC_WEBHOOK_SECRET || !(await signedBy(env.BMC_WEBHOOK_SECRET, body, signature))) return undefined;
  const event = JSON.parse(body);
  const what = {
    'donation.created': 'bought a coffee',
    'membership.started': 'became a member',
    'recurring_donation.started': 'became a monthly supporter',
    'extra_purchase.created': 'bought an extra',
    'commission_order.created': 'ordered a commission',
    'wishlist_payment.created': 'chipped in on a wishlist item',
  }[event.type];
  if (!what) return null;
  const d = event.data || {};
  const named = !d.supporter_name_type || d.supporter_name_type === 'default';
  const hidden = String(d.note_hidden) === 'true';
  return {
    platform: 'Buy Me a Coffee',
    who: named && d.supporter_name,
    what,
    message: !hidden && (d.message || d.support_note),
    test: event.live_mode === false,
  };
}

async function signedBy(secret, body, signature) {
  const enc = new TextEncoder();
  const key = await crypto.subtle.importKey('raw', enc.encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  const mac = new Uint8Array(await crypto.subtle.sign('HMAC', key, enc.encode(body)));
  const hex = [...mac].map((b) => b.toString(16).padStart(2, '0')).join('');
  if (hex.length !== signature.length) return false;
  let diff = 0;
  for (let i = 0; i < hex.length; i++) diff |= hex.charCodeAt(i) ^ signature.charCodeAt(i);
  return diff === 0;
}

function escape(text) {
  return String(text).replace(/([\\*_~`|>#[\]<])/g, '\\$1');
}

function discordMessage({ platform, who, what, message, test }) {
  let text = `**${who ? escape(String(who).slice(0, 80)) : 'Someone'}** ${what} on [${platform}](${PAGES[platform]}). Thank you! ☕`;
  if (message) text += `\n> ${escape(String(message).trim().slice(0, 1000)).replace(/\n/g, '\n> ')}`;
  return {
    username: 'Supporters',
    avatar_url: ICON,
    allowed_mentions: { parse: [] },
    embeds: [{ description: (test ? '[test] ' : '') + text, color: COLORS[platform] }],
  };
}
