# MorphDemo benchmark

Raw runs of the MorphDemo benchmark: three native audiovisual real-time
programs of exactly 65,536, 16,384 and 4,096 bytes, built by an AI model in a
single run.

## Task

Every model received the same inputs:

- [`SPEC.md`](SPEC.md): the assignment
- [`ENVIRONMENT.json`](ENVIRONMENT.json): the measured reference host
- [`prompt.txt`](prompt.txt): the prompt that started the run

## Runs

One folder per model. Each folder contains the files of the model's workspace
at the end of its run, including its own copy of the inputs above.

| Folder | Model | Run |
|---|---|---|
| [`Claude-Opus-5.5/`](Claude-Opus-5.5/) | Claude Opus 5.5 | 2026-09-23 |
| [`GPT-6-Astra/`](GPT-6-Astra/) | GPT-6 Astra | 2026-09-26 |
| [`Claude-Sonnet-5.5/`](Claude-Sonnet-5.5/) | Claude Sonnet 5.5 | 2026-09-29 |
| [`GPT-6.1-Sol/`](GPT-6.1-Sol/) | GPT-6.1 Sol | 2026-09-29 |
| [`Mistral-Large-4/`](Mistral-Large-4/) | Mistral Large 4 | 2026-10-06 |
| [`Gemini-4-Argon/`](Gemini-4-Argon/) | Gemini 4 Argon | 2026-10-14 |
| [`Kimi-K3.1-Pro/`](Kimi-K3.1-Pro/) | Kimi K3.1 Pro | 2026-10-23 |

Build outputs, caches, IDE state and harness configuration are left out (see
[`.gitignore`](.gitignore)); a `.gitignore` written by a model applies to its
folder.

## License

MIT, see [LICENSE](LICENSE). Third-party components keep their own licenses.
