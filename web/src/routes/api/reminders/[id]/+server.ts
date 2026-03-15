import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import * as store from '$lib/server/store';
import { nudgeEsp32 } from '$lib/server/esp32';

export const DELETE: RequestHandler = async ({ params }) => {
  const removed = store.remove(params.id);
  if (!removed) {
    return json({ error: 'Not found' }, { status: 404 });
  }

  nudgeEsp32();
  return new Response(null, { status: 204 });
};
